/*
 OGL_SHADER.CPP
 
 Copyright (C) 2009 by Clemens Unterkofler and the Aleph One developers
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 This license is contained in the file "COPYING",
 which is included with this source code; it is available online at
 http://www.gnu.org/licenses/gpl.html
 
 Implements OpenGL vertex/fragment shader class
 */
#include <algorithm>
#include <iostream>

#include "OGL_Shader.h"
#include "FileHandler.h"
#include "OGL_Setup.h"
#include "InfoTree.h"
#include "Logging.h"

#ifdef HAVE_OPENGL

#ifdef __SWITCH__
#include "OGL_CoreProfile.h"
#include <glm/gtc/type_ptr.hpp>

// Map ARB shader API to core GL 4.3 equivalents
#define glCreateShaderObjectARB(t)       glCreateShader(t)
#define glShaderSourceARB                glShaderSource
#define glCompileShaderARB               glCompileShader
#define glGetObjectParameterivARB(o,p,v) glGetShaderiv(o,p,v)
#define glCreateProgramObjectARB()       glCreateProgram()
#define glAttachObjectARB                glAttachShader
#define glLinkProgramARB                 glLinkProgram
#define glUseProgramObjectARB            glUseProgram
#define glDeleteObjectARB                glDeleteShader   // for shader objects only
#define glUniform1iARB                   glUniform1i
#define glUniform1fARB                   glUniform1f
#define glUniformMatrix4fvARB            glUniformMatrix4fv

// GLSL compatibility preamble — prepended to every vertex shader
static const char kSwitchVertPreamble[] =
    "#version 430 core\n"
    "layout(location=0) in vec4 a_position;\n"
    "layout(location=1) in vec4 a_color;\n"
    "layout(location=2) in vec4 a_texcoord0;\n"
    "layout(location=3) in vec3 a_normal;\n"
    "layout(location=4) in vec4 a_texcoord1;\n"
    "#define attribute in\n"
    "#define varying out\n"
    "#define gl_Vertex     a_position\n"
    "#define gl_Color      a_color\n"
    "#define gl_Normal     a_normal\n"
    "#define gl_MultiTexCoord0 a_texcoord0\n"
    "#define gl_MultiTexCoord1 a_texcoord1\n"
    "uniform mat4 u_mvpMatrix;\n"
    "uniform mat4 u_mvMatrix;\n"
    "uniform mat4 u_mvMatrixInverse;\n"
    "uniform mat3 u_normalMatrix;\n"
    "uniform mat4 u_texMatrix[2];\n"
    "#define gl_ModelViewProjectionMatrix u_mvpMatrix\n"
    "#define gl_ModelViewMatrix           u_mvMatrix\n"
    "#define gl_ModelViewMatrixInverse    u_mvMatrixInverse\n"
    "#define gl_NormalMatrix              u_normalMatrix\n"
    "#define gl_TextureMatrix             u_texMatrix\n"
    "out vec4 v_texcoord[2];\n"
    "#define gl_TexCoord v_texcoord\n"
    "uniform vec4 u_clipPlane[6];\n"
    "#define main sw_originalVertMain\n";

// GLSL compatibility preamble — prepended to every fragment shader
// Uses a main()-rename trick to inject alpha test logic:
//   - Original shader's main() is renamed to sw_originalMain()
//   - gl_FragColor writes to an intermediate vec4 (sw_tempColor)
//   - The real main() in the epilogue calls sw_originalMain(), applies
//     alpha test via discard, then writes to the actual output.
static const char kSwitchFragPreamble[] =
    "#version 430 core\n"
    "#define varying in\n"
    "#define texture2D    texture\n"
    "#define texture2DRect texture\n"
    "#define sampler2DRect sampler2D\n"
    "out vec4 sw_fragColor;\n"
    "vec4 sw_tempColor = vec4(0.0);\n"
    "#define gl_FragColor sw_tempColor\n"
    "in vec4 v_texcoord[2];\n"
    "#define gl_TexCoord v_texcoord\n"
    // gl_Fog built-in is removed in core profile.
    // Declare a plain uniform struct and alias it via preprocessor.
    "struct FogParameters { vec4 color; float density, start, end; };\n"
    "uniform FogParameters u_fog;\n"
    "#define gl_Fog u_fog\n"
    // Alpha test uniforms (GL_ALPHA_TEST is not a core profile state;
    // the shim tracks it in g_glState but shaders must implement discard)
    "uniform bool sw_alphaTestEnabled;\n"
    "uniform float sw_alphaRef;\n"
    // Rename the shader's main() so the epilogue can wrap it
    "#define main sw_originalMain\n";

// GLSL epilogue — appended after every fragment shader source.
// Provides the real main() that wraps the original and applies alpha test.
static const char kSwitchFragEpilogue[] =
    "\n#undef main\n"
    "void main(void) {\n"
    "    sw_originalMain();\n"
    "    if (sw_alphaTestEnabled && sw_tempColor.a <= sw_alphaRef) discard;\n"
    "    sw_fragColor = sw_tempColor;\n"
    "}\n";

// Vertex epilogue — wraps the vertex shader's main() to compute gl_ClipDistance
// from eye-space clip plane equations. The preamble renames main to
// sw_originalVertMain; this epilogue provides the real main().
static const char kSwitchVertEpilogue[] =
    "\n#undef main\n"
    "void main() {\n"
    "    sw_originalVertMain();\n"
    "    vec4 sw_eyePos = u_mvMatrix * a_position;\n"
    "    gl_ClipDistance[0] = dot(u_clipPlane[0], sw_eyePos);\n"
    "    gl_ClipDistance[1] = dot(u_clipPlane[1], sw_eyePos);\n"
    "    gl_ClipDistance[2] = dot(u_clipPlane[2], sw_eyePos);\n"
    "    gl_ClipDistance[3] = dot(u_clipPlane[3], sw_eyePos);\n"
    "    gl_ClipDistance[4] = dot(u_clipPlane[4], sw_eyePos);\n"
    "    gl_ClipDistance[5] = dot(u_clipPlane[5], sw_eyePos);\n"
    "}\n";

#endif // __SWITCH__

// not supported on core profile
#if defined(__SWITCH__)
static bool DisableClipVertex() { return true; }
// gl_clipvertex puts Radeons into software mode on Mac
#elif defined(__APPLE__) && defined(__MACH__)
static bool DisableClipVertex()
{
    const GLubyte* renderer = glGetString(GL_RENDERER);
    return (renderer && strncmp(reinterpret_cast<const char*>(renderer), "AMD", 3) == 0);
}
#else
static bool DisableClipVertex()
{
    return false;
}
#endif


static std::map<std::string, std::string> defaultVertexPrograms;
static std::map<std::string, std::string> defaultFragmentPrograms;
void initDefaultPrograms();

std::vector<Shader> Shader::_shaders;

const char* Shader::_uniform_names[NUMBER_OF_UNIFORM_LOCATIONS] = 
{
	"texture0",
	"texture1",
	"texture2",
	"texture3",
	"time",
	"pulsate",
	"wobble",
	"flare",
	"bloomScale",
	"bloomShift",
	"repeat",
	"offsetx",
	"offsety",
	"pass",
	"fogMix",
	"visibility",
    "transferFadeOut",
	"depth",
	"strictDepthMode",
	"glow",
	"landscapeInverseMatrix",
	"scalex",
	"scaley",
	"yaw",
	"pitch",
	"selfLuminosity",
	"gammaAdjust",
	"logicalWidth",
	"logicalHeight",
	"pixelWidth",
	"pixelHeight",
	"fogMode"
};

const char* Shader::_shader_names[NUMBER_OF_SHADER_TYPES] = 
{
	"error",
    "blur",
	"bloom",
	"landscape",
	"landscape_bloom",
	"landscape_infravision",
	"sprite",
	"sprite_bloom",
	"sprite_infravision",
	"invincible",
	"invincible_bloom",
	"invisible",
	"invisible_bloom",
	"wall",
	"wall_bloom",
	"wall_infravision",
	"bump",
	"bump_bloom",
	"gamma",
	"landscape_sphere",
	"landscape_sphere_bloom",
	"landscape_sphere_infravision"
};


class Shader_MML_Parser {
public:
	static void reset();
	static void parse(const InfoTree& root);
};

void Shader_MML_Parser::reset()
{
	Shader::_shaders.clear();
}

void Shader_MML_Parser::parse(const InfoTree& root)
{
	std::string name;
	if (!root.read_attr("name", name))
		return;
	
	for (int i = 0; i < Shader::NUMBER_OF_SHADER_TYPES; ++i) {
		if (name == Shader::_shader_names[i]) {
			initDefaultPrograms();
			Shader::loadAll();
			
			FileSpecifier vert, frag;
			root.read_path("vert", vert);
			root.read_path("frag", frag);
			int16 passes;
			root.read_attr("passes", passes);
			
			Shader::_shaders[i] = Shader(name, vert, frag, passes);
			break;
		}
	}
}

void reset_mml_opengl_shader()
{
	Shader_MML_Parser::reset();
}

void parse_mml_opengl_shader(const InfoTree& root)
{
	Shader_MML_Parser::parse(root);
}

void parseFile(FileSpecifier& fileSpec, std::string& s) {

	s.clear();

	if (fileSpec == FileSpecifier() || !fileSpec.Exists()) {
		return;
	}

	OpenedFile file;
	if (!fileSpec.Open(file))
	{
		fprintf(stderr, "%s not found\n", fileSpec.GetPath());
		return;
	}

	int32 length;
	file.GetLength(length);

	s.resize(length);
	file.Read(length, &s[0]);
}


GLhandleARB parseShader(const GLcharARB* str, GLenum shaderType) {

	GLint status;
	GLhandleARB shader = glCreateShaderObjectARB(shaderType);

	std::vector<const GLcharARB*> source;

#ifdef __SWITCH__
	if (shaderType == GL_VERTEX_SHADER)
		source.push_back(kSwitchVertPreamble);
	else
		source.push_back(kSwitchFragPreamble);
#endif

        if (DisableClipVertex()) {
            source.push_back("#define DISABLE_CLIP_VERTEX\n");
        }
	if (Wanting_sRGB)
	{
		source.push_back("#define GAMMA_CORRECTED_BLENDING\n");
	}
	if (Bloom_sRGB)
	{
		source.push_back("#define BLOOM_SRGB_FRAMEBUFFER\n");
	}
	source.push_back(str);

#ifdef __SWITCH__
	if (shaderType == GL_VERTEX_SHADER)
		source.push_back(kSwitchVertEpilogue);
	if (shaderType == GL_FRAGMENT_SHADER)
		source.push_back(kSwitchFragEpilogue);
#endif

	glShaderSourceARB(shader, source.size(), &source[0], NULL);

	glCompileShaderARB(shader);
	glGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &status);

	if(status) {
		return shader;
	} else {
        GLint infoLen = 0;
        glGetShaderiv((GLuint)(size_t)shader, GL_INFO_LOG_LENGTH, &infoLen);
        
        if(infoLen > 1)
        {
            char* infoLog = (char*) malloc(sizeof(char) * infoLen);
            glGetShaderInfoLog((GLuint)(size_t)shader, infoLen, NULL, infoLog);
            logError("Error compiling shader:\n%s\n", infoLog);
            free(infoLog);
        }
        
		glDeleteObjectARB(shader);
		return 0;
	}
}

void Shader::loadAll() {
	initDefaultPrograms();
	if (!_shaders.size()) 
	{
		_shaders.reserve(NUMBER_OF_SHADER_TYPES);
		for (int i = 0; i < NUMBER_OF_SHADER_TYPES; ++i) 
		{
			_shaders.push_back(Shader(_shader_names[i]));
		}
	}
}

void Shader::unloadAll() {
	for (int i = 0; i < _shaders.size(); ++i) 
	{
		_shaders[i].unload();
	}
}

Shader::Shader(const std::string& name) : _programObj(0), _passes(-1), _loaded(false) {
    initDefaultPrograms();
    if (defaultVertexPrograms.count(name) > 0) {
	    _vert = defaultVertexPrograms[name];
    }
    if (defaultFragmentPrograms.count(name) > 0) {
	    _frag = defaultFragmentPrograms[name];
    }
}    

Shader::Shader(const std::string& name, FileSpecifier& vert, FileSpecifier& frag, int16& passes) : _programObj(0), _passes(passes), _loaded(false) {
	initDefaultPrograms();
	
	parseFile(vert,  _vert);
	if (_vert.empty() && defaultVertexPrograms.count(name) > 0) 
	{
		_vert = defaultVertexPrograms[name];
	}
	
	parseFile(frag, _frag);
	if (_frag.empty() && defaultFragmentPrograms.count(name) > 0) 
	{
		_frag = defaultFragmentPrograms[name];
	}
}

void Shader::init() {

	std::fill_n(_uniform_locations, static_cast<int>(NUMBER_OF_UNIFORM_LOCATIONS), -1);
	std::fill_n(_cached_floats, static_cast<int>(NUMBER_OF_UNIFORM_LOCATIONS), 0.0);

#ifdef __SWITCH__
	_switchUniformsQueried = false;
	_uMVP = _uMV = _uMVInv = _uNormal = _uTexMat = -1;
	_uFogColor = _uFogDensity = _uFogStart = _uFogEnd = -1;
	_uAlphaTestEnabled = _uAlphaRef = -1;
	for (int i = 0; i < 6; i++) _uClipPlane[i] = -1;
#endif

	_loaded = true;

	_programObj = glCreateProgramObjectARB();

	assert(!_vert.empty());
	GLhandleARB vertexShader = parseShader(_vert.c_str(), GL_VERTEX_SHADER_ARB);
    if(!vertexShader) {
        _vert = defaultVertexPrograms["error"];
        vertexShader = parseShader(_vert.c_str(), GL_VERTEX_SHADER_ARB);
    }
	
	glAttachObjectARB(_programObj, vertexShader);
	glDeleteObjectARB(vertexShader);

	assert(!_frag.empty());
	GLhandleARB fragmentShader = parseShader(_frag.c_str(), GL_FRAGMENT_SHADER_ARB);
	if(!fragmentShader) {
        _frag = defaultFragmentPrograms["error"];
        fragmentShader = parseShader(_frag.c_str(), GL_FRAGMENT_SHADER_ARB);
    }
    
	glAttachObjectARB(_programObj, fragmentShader);
	glDeleteObjectARB(fragmentShader);
	
	glLinkProgramARB(_programObj);
    
    GLint linked;
    glGetProgramiv((GLuint)(size_t)_programObj, GL_LINK_STATUS, &linked);
    if(!linked)
    {
      GLint infoLen = 0;
      glGetProgramiv((GLuint)(size_t)_programObj, GL_INFO_LOG_LENGTH, &infoLen);
      if(infoLen > 1)
      {
        char* infoLog = (char*) malloc(sizeof(char) * infoLen);
        glGetProgramInfoLog((GLuint)(size_t)_programObj, infoLen, NULL, infoLog);
        logError("Error linking program:\n%s\n", infoLog);
        free(infoLog);
      }
      glDeleteProgram((GLuint)(size_t)_programObj);
    }

	assert(_programObj);

	glUseProgramObjectARB(_programObj);

	glUniform1iARB(getUniformLocation(U_Texture0), 0);
	glUniform1iARB(getUniformLocation(U_Texture1), 1);
	glUniform1iARB(getUniformLocation(U_Texture2), 2);
	glUniform1iARB(getUniformLocation(U_Texture3), 3);	

	glUseProgramObjectARB(0);

//	assert(glGetError() == GL_NO_ERROR);
}

void Shader::setFloat(UniformName name, float f) {

	if (_cached_floats[name] != f) {
		_cached_floats[name] = f;
		glUniform1fARB(getUniformLocation(name), f);
	}
}

void Shader::setMatrix4(UniformName name, float *f) {

	glUniformMatrix4fvARB(getUniformLocation(name), 1, false, f);
}

Shader::~Shader() {
	unload();
}

void Shader::enable() {
	if(!_loaded) { init(); }
	glUseProgramObjectARB(_programObj);
#ifdef __SWITCH__
	uploadSwitchMatrices();
#endif
}

void Shader::disable() {
	glUseProgramObjectARB(0);
}

void Shader::unload() {
	if(_programObj) {
#ifdef __SWITCH__
		glDeleteProgram(_programObj);
#else
		glDeleteObjectARB(_programObj);
#endif
		_programObj = 0;
		_loaded = false;
	}
}

int16 Shader::passes() {
	return _passes;
}

#ifdef __SWITCH__
void Shader::uploadSwitchMatrices() {
	if (!_switchUniformsQueried) {
		_switchUniformsQueried = true;
		_uMVP    = glGetUniformLocation(_programObj, "u_mvpMatrix");
		_uMV     = glGetUniformLocation(_programObj, "u_mvMatrix");
		_uMVInv  = glGetUniformLocation(_programObj, "u_mvMatrixInverse");
		_uNormal = glGetUniformLocation(_programObj, "u_normalMatrix");
		_uTexMat = glGetUniformLocation(_programObj, "u_texMatrix");
		_uFogColor   = glGetUniformLocation(_programObj, "u_fog.color");
		_uFogDensity = glGetUniformLocation(_programObj, "u_fog.density");
		_uFogStart   = glGetUniformLocation(_programObj, "u_fog.start");
		_uFogEnd     = glGetUniformLocation(_programObj, "u_fog.end");
		_uAlphaTestEnabled = glGetUniformLocation(_programObj, "sw_alphaTestEnabled");
		_uAlphaRef   = glGetUniformLocation(_programObj, "sw_alphaRef");
		for (int i = 0; i < 6; i++) {
			char name[32];
			snprintf(name, sizeof(name), "u_clipPlane[%d]", i);
			_uClipPlane[i] = glGetUniformLocation(_programObj, name);
		}
	}
	if (_uMVP    >= 0) glUniformMatrix4fv(_uMVP,    1, GL_FALSE, glm::value_ptr(g_matrixStack.getMVP()));
	if (_uMV     >= 0) glUniformMatrix4fv(_uMV,     1, GL_FALSE, glm::value_ptr(g_matrixStack.getMV()));
	if (_uMVInv  >= 0) glUniformMatrix4fv(_uMVInv,  1, GL_FALSE, glm::value_ptr(g_matrixStack.getMVInverse()));
	if (_uNormal >= 0) glUniformMatrix3fv(_uNormal, 1, GL_FALSE, glm::value_ptr(g_matrixStack.getNormal()));
	if (_uTexMat >= 0) glUniformMatrix4fv(_uTexMat, 1, GL_FALSE, glm::value_ptr(g_matrixStack.getTextureMatrix()));
	if (_uFogColor   >= 0) glUniform4fv(_uFogColor,   1, g_glState.fogColor);
	if (_uFogDensity >= 0) glUniform1f(_uFogDensity,  g_glState.fogDensity);
	if (_uFogStart   >= 0) glUniform1f(_uFogStart,    g_glState.fogStart);
	if (_uFogEnd     >= 0) glUniform1f(_uFogEnd,      g_glState.fogEnd);
	// Alpha test state — core profile has no hardware alpha test;
	// the fragment epilogue implements it via discard
	if (_uAlphaTestEnabled >= 0) glUniform1i(_uAlphaTestEnabled, (int)g_glState.alphaTestEnabled);
	if (_uAlphaRef   >= 0) glUniform1f(_uAlphaRef,    g_glState.alphaRef);
	// Clip plane equations (eye-space, set by SwitchClipPlane)
	for (int i = 0; i < 6; i++) {
		if (_uClipPlane[i] >= 0)
			glUniform4fv(_uClipPlane[i], 1, glm::value_ptr(g_glState.clipPlane[i]));
	}
}
#endif // __SWITCH__

void initDefaultPrograms() {
    if (defaultVertexPrograms.size() > 0)
        return;
    
    
    defaultVertexPrograms["error"] = ""
    "varying vec4 vertexColor;\n"
    "void main(void) {\n"
    "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
    "    vertexColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
    "}\n";
    defaultFragmentPrograms["error"] = ""
    "float round(float n){ \n"
    "   float nSign = 1.0; \n"
    "   if ( n < 0.0 ) { nSign = -1.0; }; \n"
    "   return nSign * floor(abs(n)+0.5); \n"
    "} \n"
    "void main (void) {\n"
    "    gl_FragColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
    "    float checkerSize = 8.0;\n"
    "    float phase = 0.0;\n"
    "    if( mod(round(gl_FragCoord.y / checkerSize), 2.0) == 0.0) {\n"
    "       phase = checkerSize;\n"
    "    }\n"
    "    if (mod(round((gl_FragCoord.x + phase) / checkerSize), 2.0)==0.0) {\n"
    "       gl_FragColor.a = 0.5;\n"
    "    }\n"
    "}\n";
    
	defaultVertexPrograms["gamma"] = ""
	"varying vec4 vertexColor;\n"
	"void main(void) {\n"
	"	gl_TexCoord[0] = gl_MultiTexCoord0;\n"
	"	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
	"	vertexColor = gl_Color;\n"
	"}\n";
	defaultFragmentPrograms["gamma"] = ""
	"uniform sampler2DRect texture0;\n"
	"uniform float gammaAdjust;\n"
	"void main (void) {\n"
	"	vec4 color0 = texture2DRect(texture0, gl_TexCoord[0].xy);\n"
	"	gl_FragColor = vec4(pow(color0.r, gammaAdjust), pow(color0.g, gammaAdjust), pow(color0.b, gammaAdjust), 1.0);\n"
	"}\n";
	
    defaultVertexPrograms["blur"] = ""
        "varying vec4 vertexColor;\n"
        "void main(void) {\n"
        "	gl_TexCoord[0] = gl_MultiTexCoord0;\n"
        "	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
        "	vertexColor = gl_Color;\n"
        "}\n";
    defaultFragmentPrograms["blur"] = ""
        "uniform sampler2DRect texture0;\n"
        "uniform float offsetx;\n"
        "uniform float offsety;\n"
        "uniform float pass;\n"
        "varying vec4 vertexColor;\n"
        "const float f0 = 0.14012035;\n"
        "const float f1 = 0.24122258;\n"
        "const float o1 = 1.45387071;\n"
        "const float f2 = 0.13265595;\n"
        "const float o2 = 3.39370426;\n"
        "const float f3 = 0.04518872;\n"
        "const float o3 = 5.33659787;\n"
        "#ifdef BLOOM_SRGB_FRAMEBUFFER\n"
        "vec3 s2l(vec3 srgb) { return srgb; }\n"
        "vec3 l2s(vec3 linear) { return linear; }\n"
        "#else\n"
        "vec3 s2l(vec3 srgb) { return srgb * srgb; }\n"
        "vec3 l2s(vec3 linear) { return sqrt(linear); }\n"
        "#endif\n"
        "void main (void) {\n"
        "	vec2 s = vec2(offsetx, offsety);\n"
        "	// Thanks to Renaud Bedard - http://theinstructionlimit.com/?p=43\n"
        "	vec3 c = s2l(texture2DRect(texture0, gl_TexCoord[0].xy).rgb);\n"
        "	vec3 t = f0 * c;\n"
        "	t += f1 * s2l(texture2DRect(texture0, gl_TexCoord[0].xy - o1*s).rgb);\n"
        "	t += f1 * s2l(texture2DRect(texture0, gl_TexCoord[0].xy + o1*s).rgb);\n"
        "	t += f2 * s2l(texture2DRect(texture0, gl_TexCoord[0].xy - o2*s).rgb);\n"
        "	t += f2 * s2l(texture2DRect(texture0, gl_TexCoord[0].xy + o2*s).rgb);\n"
        "	t += f3 * s2l(texture2DRect(texture0, gl_TexCoord[0].xy - o3*s).rgb);\n"
        "	t += f3 * s2l(texture2DRect(texture0, gl_TexCoord[0].xy + o3*s).rgb);\n"
        "	gl_FragColor = vec4(l2s(t), 1.0) * vertexColor;\n"
        "}\n";    
    
    defaultVertexPrograms["bloom"] = ""
        "varying vec4 vertexColor;\n"
        "void main(void) {\n"
        "	gl_TexCoord[0] = gl_MultiTexCoord0;\n"
        "	gl_TexCoord[1] = gl_MultiTexCoord1;\n"
        "	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
        "	vertexColor = gl_Color;\n"
        "}\n";
    defaultFragmentPrograms["bloom"] = ""
        "uniform sampler2DRect texture0;\n"
        "uniform sampler2DRect texture1;\n"
        "uniform float pass;\n"
        "varying vec4 vertexColor;\n"
        "vec3 s2l(vec3 srgb) { return srgb * srgb; }\n"
        "vec3 l2s(vec3 linear) { return sqrt(linear); }\n"
		"#ifndef BLOOM_SRGB_FRAMEBUFFER\n"
	    "vec3 b2l(vec3 bloom) { return bloom * bloom; }\n"
		"#else\n"
		"vec3 b2l(vec3 bloom) { return bloom; }\n"
        "#endif\n"
        "void main (void) {\n"
        "	vec4 color0 = texture2DRect(texture0, gl_TexCoord[0].xy);\n"
        "	vec4 color1 = texture2DRect(texture1, gl_TexCoord[1].xy);\n"
        "	vec3 color = l2s(s2l(color0.rgb) + b2l(color1.rgb));\n"
        "	gl_FragColor = vec4(color, 1.0);\n"
        "}\n";

	defaultVertexPrograms["landscape"] =
        #include "Shaders/landscape.vert"
		;
	defaultFragmentPrograms["landscape"] =
		#include "Shaders/landscape.frag"
		;
	
    defaultVertexPrograms["landscape_bloom"] = defaultVertexPrograms["landscape"];
	defaultFragmentPrograms["landscape_bloom"] =
		#include "Shaders/landscape_bloom.frag"
		;
	
	defaultVertexPrograms["landscape_infravision"] = defaultVertexPrograms["landscape"];
	defaultFragmentPrograms["landscape_infravision"] =
        #include "Shaders/landscape_infravision.frag"
		;

	defaultVertexPrograms["sprite"] =
        #include "Shaders/sprite.vert"
		;
	defaultFragmentPrograms["sprite"] =
        #include "Shaders/sprite.frag"
		;
	
    defaultVertexPrograms["sprite_bloom"] = defaultVertexPrograms["sprite"];
	defaultFragmentPrograms["sprite_bloom"] =
		#include "Shaders/sprite_bloom.frag"
		;

	defaultVertexPrograms["sprite_infravision"] = defaultVertexPrograms["sprite"];
	defaultFragmentPrograms["sprite_infravision"] =
        #include "Shaders/sprite_infravision.frag"
		;
	
    defaultVertexPrograms["invincible"] = defaultVertexPrograms["sprite"];
	defaultFragmentPrograms["invincible"] =
		#include "Shaders/invincible.frag"
		;
	
    defaultVertexPrograms["invincible_bloom"] = defaultVertexPrograms["invincible"];
	defaultFragmentPrograms["invincible_bloom"] =
        #include "Shaders/invincible_bloom.frag"
		;

    defaultVertexPrograms["invisible"] = defaultVertexPrograms["sprite"];
	defaultFragmentPrograms["invisible"] =
        #include "Shaders/invisible.frag"
		;
    defaultVertexPrograms["invisible_bloom"] = defaultVertexPrograms["invisible"];
	defaultFragmentPrograms["invisible_bloom"] =
        #include "Shaders/invisible_bloom.frag"
		;

	defaultVertexPrograms["wall"] =
        #include "Shaders/wall.vert"
		;
	defaultFragmentPrograms["wall"] =
        #include "Shaders/wall.frag"
		;
	
    defaultVertexPrograms["wall_bloom"] = defaultVertexPrograms["wall"];
	defaultFragmentPrograms["wall_bloom"] =
		#include "Shaders/wall_bloom.frag"
		;
	
	defaultVertexPrograms["wall_infravision"] = defaultVertexPrograms["wall"];
	defaultFragmentPrograms["wall_infravision"] =
        #include "Shaders/wall_infravision.frag"
		;
    
    defaultVertexPrograms["bump"] = defaultVertexPrograms["wall"];
	defaultFragmentPrograms["bump"] =
        #include "Shaders/bump.frag"
		;
	
    defaultVertexPrograms["bump_bloom"] = defaultVertexPrograms["bump"];
    defaultFragmentPrograms["bump_bloom"] =
        #include "Shaders/bump_bloom.frag"
		;

	defaultVertexPrograms["landscape_sphere"] =
		#include "Shaders/landscape_sphere.vert"
	;

	defaultFragmentPrograms["landscape_sphere"] =
		#include "Shaders/landscape_sphere.frag"
	;

	defaultVertexPrograms["landscape_sphere_bloom"] = defaultVertexPrograms["landscape_sphere"];
	defaultFragmentPrograms["landscape_sphere_bloom"] =
		#include "Shaders/landscape_sphere_bloom.frag"
	;

	defaultVertexPrograms["landscape_sphere_infravision"] = defaultVertexPrograms["landscape_sphere"];
	defaultFragmentPrograms["landscape_sphere_infravision"] =
		#include "Shaders/landscape_sphere_infravision.frag"
	;
}

#endif
