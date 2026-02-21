/*
	OGL_CoreProfile.cpp — Switch OpenGL 4.3 Core Profile compatibility shim.
	See OGL_CoreProfile.h for design notes.
*/

#ifdef __SWITCH__

#include "OGL_CoreProfile.h"
#include <cstring>
#include <cstdio>
#include <glm/gtc/matrix_inverse.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────────────────────────────────────

SwitchMatrixStack      g_matrixStack;
SwitchGLState          g_glState;
SwitchImmediateRenderer g_immediateRenderer;

// ─────────────────────────────────────────────────────────────────────────────
// SwitchMatrixStack
// ─────────────────────────────────────────────────────────────────────────────

SwitchMatrixStack::SwitchMatrixStack()
	: _mode(GL_MODELVIEW), _texMatrix(1.f)
{
	_mvStack.push_back(glm::mat4(1.f));
	_projStack.push_back(glm::mat4(1.f));
}

std::vector<glm::mat4>& SwitchMatrixStack::active()
{
	return (_mode == GL_PROJECTION) ? _projStack : _mvStack;
}
const std::vector<glm::mat4>& SwitchMatrixStack::active() const
{
	return (_mode == GL_PROJECTION) ? _projStack : _mvStack;
}

void SwitchMatrixStack::matrixMode(GLenum mode) { _mode = mode; }
GLenum SwitchMatrixStack::getMode() const { return _mode; }

void SwitchMatrixStack::loadIdentity()
{
	if (_mode == GL_TEXTURE) { _texMatrix = glm::mat4(1.f); return; }
	active().back() = glm::mat4(1.f);
}

void SwitchMatrixStack::pushMatrix()
{
	if (_mode == GL_TEXTURE) return;
	auto &s = active();
	s.push_back(s.back());
}

void SwitchMatrixStack::popMatrix()
{
	if (_mode == GL_TEXTURE) return;
	auto &s = active();
	if (s.size() > 1) s.pop_back();
}

void SwitchMatrixStack::translatef(GLfloat x, GLfloat y, GLfloat z)
{
	if (_mode == GL_TEXTURE) { _texMatrix = glm::translate(_texMatrix, glm::vec3(x,y,z)); return; }
	active().back() = glm::translate(active().back(), glm::vec3(x,y,z));
}
void SwitchMatrixStack::translated(GLdouble x, GLdouble y, GLdouble z)
{
	translatef((float)x, (float)y, (float)z);
}

void SwitchMatrixStack::rotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	glm::mat4 r = glm::rotate(glm::mat4(1.f), glm::radians(angle), glm::vec3(x,y,z));
	if (_mode == GL_TEXTURE) { _texMatrix = _texMatrix * r; return; }
	active().back() = active().back() * r;
}
void SwitchMatrixStack::rotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
	rotatef((float)angle, (float)x, (float)y, (float)z);
}

void SwitchMatrixStack::scalef(GLfloat x, GLfloat y, GLfloat z)
{
	if (_mode == GL_TEXTURE) { _texMatrix = glm::scale(_texMatrix, glm::vec3(x,y,z)); return; }
	active().back() = glm::scale(active().back(), glm::vec3(x,y,z));
}
void SwitchMatrixStack::scaled(GLdouble x, GLdouble y, GLdouble z)
{
	scalef((float)x, (float)y, (float)z);
}

static glm::mat4 fromDoubleArray(const GLdouble *m)
{
	// GL matrices are column-major
	glm::mat4 r;
	for (int c = 0; c < 4; c++)
		for (int rr = 0; rr < 4; rr++)
			r[c][rr] = (float)m[c*4+rr];
	return r;
}

void SwitchMatrixStack::loadMatrixd(const GLdouble *m)
{
	active().back() = fromDoubleArray(m);
}
void SwitchMatrixStack::loadMatrixf(const GLfloat *m)
{
	active().back() = glm::make_mat4(m);
}
void SwitchMatrixStack::multMatrixd(const GLdouble *m)
{
	active().back() = active().back() * fromDoubleArray(m);
}

void SwitchMatrixStack::frustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
	// Build a frustum matrix (column-major, same as glFrustum)
	float rl = (float)(r - l), tb = (float)(t - b), fn = (float)(f - n);
	glm::mat4 fr(0.f);
	fr[0][0] = 2.f*(float)n / rl;
	fr[1][1] = 2.f*(float)n / tb;
	fr[2][0] = (float)(r+l) / rl;
	fr[2][1] = (float)(t+b) / tb;
	fr[2][2] = -(float)(f+n) / fn;
	fr[2][3] = -1.f;
	fr[3][2] = -2.f*(float)f*(float)n / fn;
	active().back() = active().back() * fr;
}

void SwitchMatrixStack::ortho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
	// Same as glm::ortho but column-major consistent
	active().back() = active().back() * glm::ortho((float)l,(float)r,(float)b,(float)t,(float)n,(float)f);
}

void SwitchMatrixStack::getDoublev(GLenum pname, GLdouble *out) const
{
	const glm::mat4 *m = nullptr;
	if (pname == GL_MODELVIEW_MATRIX)  m = &_mvStack.back();
	if (pname == GL_PROJECTION_MATRIX) m = &_projStack.back();
	if (!m) return;
	for (int c = 0; c < 4; c++)
		for (int rr = 0; rr < 4; rr++)
			out[c*4+rr] = (GLdouble)(*m)[c][rr];
}
void SwitchMatrixStack::getFloatv(GLenum pname, GLfloat *out) const
{
	const glm::mat4 *m = nullptr;
	if (pname == GL_MODELVIEW_MATRIX)  m = &_mvStack.back();
	if (pname == GL_PROJECTION_MATRIX) m = &_projStack.back();
	if (pname == GL_TEXTURE_MATRIX)    m = &_texMatrix;
	if (!m) return;
	for (int c = 0; c < 4; c++)
		for (int rr = 0; rr < 4; rr++)
			out[c*4+rr] = (*m)[c][rr];
}

glm::mat4 SwitchMatrixStack::getMV()     const { return _mvStack.back(); }
glm::mat4 SwitchMatrixStack::getProj()   const { return _projStack.back(); }
glm::mat4 SwitchMatrixStack::getMVP()    const { return getProj() * getMV(); }
glm::mat4 SwitchMatrixStack::getMVInverse() const { return glm::inverse(getMV()); }
glm::mat3 SwitchMatrixStack::getNormal() const
{
	return glm::mat3(glm::transpose(glm::inverse(getMV())));
}
void SwitchMatrixStack::setTextureMatrix(const glm::mat4 &m) { _texMatrix = m; }
glm::mat4 SwitchMatrixStack::getTextureMatrix() const { return _texMatrix; }

// ─────────────────────────────────────────────────────────────────────────────
// SwitchGetDoublev / SwitchGetFloatv wrappers
// ─────────────────────────────────────────────────────────────────────────────

void SwitchGetDoublev(GLenum pname, GLdouble *params)
{
	if (pname == GL_MODELVIEW_MATRIX || pname == GL_PROJECTION_MATRIX) {
		g_matrixStack.getDoublev(pname, params);
	} else {
		// Fall through to real glad function for non-matrix queries
		// (undef the macro locally — not possible here, so call the function pointer directly)
		// glad stores glGetDoublev as a function pointer named glad_glGetDoublev
		extern void (GLAD_API_PTR *glad_glGetDoublev)(GLenum, GLdouble *);
		if (glad_glGetDoublev) glad_glGetDoublev(pname, params);
	}
}

void SwitchGetFloatv(GLenum pname, GLfloat *params)
{
	if (pname == GL_MODELVIEW_MATRIX || pname == GL_PROJECTION_MATRIX || pname == GL_TEXTURE_MATRIX) {
		g_matrixStack.getFloatv(pname, params);
	} else {
		extern void (GLAD_API_PTR *glad_glGetFloatv)(GLenum, GLfloat *);
		if (glad_glGetFloatv) glad_glGetFloatv(pname, params);
	}
}

void SwitchGetIntegerv(GLenum pname, GLint *params)
{
	if (pname == GL_MATRIX_MODE) {
		*params = (GLint)g_matrixStack.getMode();
	} else {
		extern void (GLAD_API_PTR *glad_glGetIntegerv)(GLenum, GLint *);
		if (glad_glGetIntegerv) glad_glGetIntegerv(pname, params);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// SwitchGLState
// ─────────────────────────────────────────────────────────────────────────────

void SwitchGLState::enable(GLenum cap)
{
	switch (cap) {
	case GL_FOG:         fogEnabled = true;  dirty = true; break;
	case GL_ALPHA_TEST:  alphaTestEnabled = true; dirty = true; break;
	case GL_TEXTURE_2D:  texture2DEnabled = true; dirty = true; break;
	// Clip planes — pass through to real GL (core profile GL_CLIP_DISTANCE)
	case GL_CLIP_DISTANCE0:
	case GL_CLIP_DISTANCE1:
	case GL_CLIP_DISTANCE2:
	case GL_CLIP_DISTANCE3:
	case GL_CLIP_DISTANCE4:
	case GL_CLIP_DISTANCE5:
		{
			extern void (GLAD_API_PTR *glad_glEnable)(GLenum);
			if (glad_glEnable) glad_glEnable(cap);
		}
		break;
	// Core-profile safe caps — pass through
	default:
		{
			extern void (GLAD_API_PTR *glad_glEnable)(GLenum);
			if (glad_glEnable) glad_glEnable(cap);
		}
		break;
	}
}

void SwitchGLState::disable(GLenum cap)
{
	switch (cap) {
	case GL_FOG:         fogEnabled = false; dirty = true; break;
	case GL_ALPHA_TEST:  alphaTestEnabled = false; dirty = true; break;
	case GL_TEXTURE_2D:  texture2DEnabled = false; dirty = true; break;
	// Core-profile-removed caps — silently ignore
	case GL_COLOR_LOGIC_OP: break;
	// Clip planes — pass through to real GL (core profile GL_CLIP_DISTANCE)
	case GL_CLIP_DISTANCE0:
	case GL_CLIP_DISTANCE1:
	case GL_CLIP_DISTANCE2:
	case GL_CLIP_DISTANCE3:
	case GL_CLIP_DISTANCE4:
	case GL_CLIP_DISTANCE5:
		{
			extern void (GLAD_API_PTR *glad_glDisable)(GLenum);
			if (glad_glDisable) glad_glDisable(cap);
		}
		break;
	default:
		{
			extern void (GLAD_API_PTR *glad_glDisable)(GLenum);
			if (glad_glDisable) glad_glDisable(cap);
		}
		break;
	}
}

void SwitchGLState::fogf(GLenum pname, GLfloat v)
{
	switch (pname) {
	case GL_FOG_START:   fogStart   = v; dirty = true; break;
	case GL_FOG_END:     fogEnd     = v; dirty = true; break;
	case GL_FOG_DENSITY: fogDensity = v; dirty = true; break;
	case GL_FOG_MODE:    fogMode    = (GLint)v; dirty = true; break;
	}
}
void SwitchGLState::fogfv(GLenum pname, const GLfloat *p)
{
	if (pname == GL_FOG_COLOR) {
		fogColor[0]=p[0]; fogColor[1]=p[1]; fogColor[2]=p[2]; fogColor[3]=p[3];
		dirty = true;
	} else fogf(pname, p[0]);
}
void SwitchGLState::fogi(GLenum pname, GLint v)
{
	switch (pname) {
	case GL_FOG_MODE: fogMode = v; dirty = true; break;
	default: fogf(pname, (float)v); break;
	}
}
void SwitchGLState::alphaTestFunc(GLenum func, GLfloat ref)
{
	alphaFunc = func; alphaRef = ref; dirty = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// SwitchClipPlane — transform equation to eye space and store
// ─────────────────────────────────────────────────────────────────────────────

void SwitchClipPlane(GLenum plane, const GLdouble *equation)
{
	int idx = (int)plane - (int)GL_CLIP_DISTANCE0;
	if (idx < 0 || idx >= 6) return;
	// Legacy glClipPlane transforms the equation by the inverse-transpose of
	// the current modelview matrix, yielding an eye-space plane equation.
	glm::mat4 mv = g_matrixStack.getMV();
	glm::mat4 mvInvTranspose = glm::transpose(glm::inverse(mv));
	glm::vec4 objPlane((float)equation[0], (float)equation[1],
	                   (float)equation[2], (float)equation[3]);
	g_glState.clipPlane[idx] = mvInvTranspose * objPlane;
}

// ─────────────────────────────────────────────────────────────────────────────
// SwitchEnable / SwitchDisable — top-level wrappers
// ─────────────────────────────────────────────────────────────────────────────

void SwitchEnable(GLenum cap)  { g_glState.enable(cap); }
void SwitchDisable(GLenum cap) { g_glState.disable(cap); }

// ─────────────────────────────────────────────────────────────────────────────
// Built-in shader source
// ─────────────────────────────────────────────────────────────────────────────

static const char *kBuiltinVert = R"GLSL(
#version 430 core
layout(location=0) in vec4 a_position;
layout(location=1) in vec4 a_color;
layout(location=2) in vec4 a_texcoord0;
layout(location=3) in vec3 a_normal;
layout(location=4) in vec4 a_texcoord1;

uniform mat4 u_mvpMatrix;
uniform mat4 u_mvMatrix;
uniform mat4 u_texMatrix;
uniform vec4 u_clipPlane[6];

out vec4 v_color;
out vec2 v_texcoord0;
out vec3 v_eyePos;

void main() {
    gl_Position  = u_mvpMatrix * a_position;
    v_color      = a_color;
    vec4 tc      = u_texMatrix * vec4(a_texcoord0.xy, 0.0, 1.0);
    v_texcoord0  = tc.xy;
    vec4 eyePos  = u_mvMatrix * a_position;
    v_eyePos     = eyePos.xyz;
    gl_ClipDistance[0] = dot(u_clipPlane[0], eyePos);
    gl_ClipDistance[1] = dot(u_clipPlane[1], eyePos);
    gl_ClipDistance[2] = dot(u_clipPlane[2], eyePos);
    gl_ClipDistance[3] = dot(u_clipPlane[3], eyePos);
    gl_ClipDistance[4] = dot(u_clipPlane[4], eyePos);
    gl_ClipDistance[5] = dot(u_clipPlane[5], eyePos);
}
)GLSL";

static const char *kBuiltinFrag = R"GLSL(
#version 430 core
in vec4 v_color;
in vec2 v_texcoord0;
in vec3 v_eyePos;

uniform sampler2D u_texture0;
uniform bool  u_texEnabled;
uniform bool  u_fogEnabled;
uniform int   u_fogMode;
uniform float u_fogStart, u_fogEnd, u_fogDensity;
uniform vec4  u_fogColor;
uniform bool  u_alphaEnabled;
uniform float u_alphaRef;

out vec4 fragColor;

void main() {
    vec4 color = v_color;
    if (u_texEnabled) {
        color *= texture(u_texture0, v_texcoord0);
    }
    if (u_alphaEnabled && color.a < u_alphaRef) discard;
    if (u_fogEnabled) {
        float dist = length(v_eyePos);
        float ff;
        if (u_fogMode == 0)
            ff = clamp((u_fogEnd - dist) / (u_fogEnd - u_fogStart), 0.0, 1.0);
        else if (u_fogMode == 1)
            ff = clamp(exp(-u_fogDensity * dist), 0.0, 1.0);
        else
            ff = clamp(exp(-u_fogDensity * u_fogDensity * dist * dist), 0.0, 1.0);
        color.rgb = mix(u_fogColor.rgb, color.rgb, ff);
    }
    fragColor = color;
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// SwitchImmediateRenderer
// ─────────────────────────────────────────────────────────────────────────────

GLuint SwitchImmediateRenderer::compileBuiltinShader(GLenum type, const char *src)
{
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);
	GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char buf[1024];
		glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
		fprintf(stderr, "[SwitchOGL] Built-in shader compile error: %s\n", buf);
	}
	return s;
}

void SwitchImmediateRenderer::init()
{
	glGenVertexArrays(1, &_vao);
	glGenBuffers(1, &_vbo);
	initBuiltinShader();
}

void SwitchImmediateRenderer::shutdown()
{
	if (_vao) { glDeleteVertexArrays(1, &_vao); _vao = 0; }
	if (_vbo) { glDeleteBuffers(1, &_vbo); _vbo = 0; }
	if (_builtinProg) { glDeleteProgram(_builtinProg); _builtinProg = 0; }
}

void SwitchImmediateRenderer::initBuiltinShader()
{
	GLuint vs = compileBuiltinShader(GL_VERTEX_SHADER,   kBuiltinVert);
	GLuint fs = compileBuiltinShader(GL_FRAGMENT_SHADER, kBuiltinFrag);
	_builtinProg = glCreateProgram();
	glAttachShader(_builtinProg, vs);
	glAttachShader(_builtinProg, fs);
	glLinkProgram(_builtinProg);
	GLint ok; glGetProgramiv(_builtinProg, GL_LINK_STATUS, &ok);
	if (!ok) {
		char buf[1024];
		glGetProgramInfoLog(_builtinProg, sizeof(buf), nullptr, buf);
		fprintf(stderr, "[SwitchOGL] Built-in program link error: %s\n", buf);
	}
	glDeleteShader(vs);
	glDeleteShader(fs);

	_uMVP        = glGetUniformLocation(_builtinProg, "u_mvpMatrix");
	_uMV         = glGetUniformLocation(_builtinProg, "u_mvMatrix");
	_uTexMat     = glGetUniformLocation(_builtinProg, "u_texMatrix");
	_uFogEnabled = glGetUniformLocation(_builtinProg, "u_fogEnabled");
	_uFogMode    = glGetUniformLocation(_builtinProg, "u_fogMode");
	_uFogStart   = glGetUniformLocation(_builtinProg, "u_fogStart");
	_uFogEnd     = glGetUniformLocation(_builtinProg, "u_fogEnd");
	_uFogDensity = glGetUniformLocation(_builtinProg, "u_fogDensity");
	_uFogColor   = glGetUniformLocation(_builtinProg, "u_fogColor");
	_uAlphaEnabled = glGetUniformLocation(_builtinProg, "u_alphaEnabled");
	_uAlphaRef   = glGetUniformLocation(_builtinProg, "u_alphaRef");
	_uTexEnabled = glGetUniformLocation(_builtinProg, "u_texEnabled");
	_uTex0       = glGetUniformLocation(_builtinProg, "u_texture0");
	for (int i = 0; i < 6; i++) {
		char name[32];
		snprintf(name, sizeof(name), "u_clipPlane[%d]", i);
		_uClipPlane[i] = glGetUniformLocation(_builtinProg, name);
	}
}

void SwitchImmediateRenderer::uploadBuiltinUniforms()
{
	glm::mat4 mvp = g_matrixStack.getMVP();
	glm::mat4 mv  = g_matrixStack.getMV();
	glm::mat4 tex = g_matrixStack.getTextureMatrix();
	glUniformMatrix4fv(_uMVP, 1, GL_FALSE, glm::value_ptr(mvp));
	glUniformMatrix4fv(_uMV,  1, GL_FALSE, glm::value_ptr(mv));
	glUniformMatrix4fv(_uTexMat, 1, GL_FALSE, glm::value_ptr(tex));

	glUniform1i(_uFogEnabled, (int)g_glState.fogEnabled);
	glUniform1i(_uFogMode,    g_glState.fogMode);
	glUniform1f(_uFogStart,   g_glState.fogStart);
	glUniform1f(_uFogEnd,     g_glState.fogEnd);
	glUniform1f(_uFogDensity, g_glState.fogDensity);
	glUniform4fv(_uFogColor,  1, g_glState.fogColor);
	glUniform1i(_uAlphaEnabled, (int)g_glState.alphaTestEnabled);
	glUniform1f(_uAlphaRef,   g_glState.alphaRef);
	glUniform1i(_uTexEnabled, (int)g_glState.texture2DEnabled);
	glUniform1i(_uTex0, 0);
	// Clip plane equations (eye-space)
	for (int i = 0; i < 6; i++) {
		if (_uClipPlane[i] >= 0)
			glUniform4fv(_uClipPlane[i], 1, glm::value_ptr(g_glState.clipPlane[i]));
	}
}

void SwitchImmediateRenderer::clientActiveTexture(GLenum unit)
{
	_activeTexUnit = (unit == GL_TEXTURE1) ? 1 : 0;
}

void SwitchImmediateRenderer::enableClientState(GLenum cap)
{
	switch (cap) {
	case GL_VERTEX_ARRAY:        _va.enabled = true;  break;
	case GL_TEXTURE_COORD_ARRAY: _ta[_activeTexUnit].enabled = true; break;
	case GL_COLOR_ARRAY:         _ca.enabled = true;  break;
	case GL_NORMAL_ARRAY:        _na.enabled = true;  break;
	default: break;
	}
}

void SwitchImmediateRenderer::disableClientState(GLenum cap)
{
	switch (cap) {
	case GL_VERTEX_ARRAY:        _va.enabled = false; break;
	case GL_TEXTURE_COORD_ARRAY: _ta[_activeTexUnit].enabled = false; break;
	case GL_COLOR_ARRAY:         _ca.enabled = false; break;
	case GL_NORMAL_ARRAY:        _na.enabled = false; break;
	default: break;
	}
}

void SwitchImmediateRenderer::vertexPointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
{
	_va.size=size; _va.type=type; _va.stride=stride; _va.ptr=ptr;
}
void SwitchImmediateRenderer::texCoordPointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
{
	_ta[_activeTexUnit].size=size; _ta[_activeTexUnit].type=type;
	_ta[_activeTexUnit].stride=stride; _ta[_activeTexUnit].ptr=ptr;
}
void SwitchImmediateRenderer::colorPointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
{
	_ca.size=size; _ca.type=type; _ca.stride=stride; _ca.ptr=ptr;
}
void SwitchImmediateRenderer::normalPointer(GLenum type, GLsizei stride, const void *ptr)
{
	_na.size=3; _na.type=type; _na.stride=stride; _na.ptr=ptr;
}

void SwitchImmediateRenderer::ensureVBO(size_t bytes)
{
	if (bytes > _vboCapacity) {
		_vboCapacity = bytes * 2;
		glBindBuffer(GL_ARRAY_BUFFER, _vbo);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)_vboCapacity, nullptr, GL_STREAM_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}

GLfloat SwitchImmediateRenderer::fetchFloat(const ArrayState &a, int idx, int component, GLfloat def) const
{
	if (!a.ptr) return def;
	int elemSize;
	switch (a.type) {
	case GL_FLOAT:          elemSize = sizeof(GLfloat); break;
	case GL_DOUBLE:         elemSize = sizeof(GLdouble); break;
	case GL_SHORT:
	case GL_UNSIGNED_SHORT: elemSize = sizeof(GLushort); break;
	case GL_UNSIGNED_BYTE:  elemSize = sizeof(GLubyte); break;
	default:                elemSize = 4; break;
	}
	int stride = a.stride ? a.stride : a.size * elemSize;
	const char *base = (const char*)a.ptr + idx * stride;
	if (a.type == GL_FLOAT) {
		const GLfloat *f = (const GLfloat*)base;
		return (component < a.size) ? f[component] : def;
	} else if (a.type == GL_SHORT || a.type == GL_UNSIGNED_SHORT) {
		const GLushort *s = (const GLushort*)base;
		return (component < a.size) ? s[component] / 32767.f : def;
	} else if (a.type == GL_UNSIGNED_BYTE) {
		const GLubyte *b = (const GLubyte*)base;
		return (component < a.size) ? b[component] / 255.f : def;
	} else if (a.type == GL_DOUBLE) {
		const GLdouble *d = (const GLdouble*)base;
		return (component < a.size) ? (GLfloat)d[component] : def;
	}
	return def;
}

void SwitchImmediateRenderer::fillVertex(Vertex &v, int idx) const
{
	// Position
	v.pos[0] = fetchFloat(_va, idx, 0, 0.f);
	v.pos[1] = fetchFloat(_va, idx, 1, 0.f);
	v.pos[2] = fetchFloat(_va, idx, 2, 0.f);
	v.pos[3] = (_va.size >= 4) ? fetchFloat(_va, idx, 3, 1.f) : 1.f;

	// Color
	if (_ca.enabled) {
		v.col[0] = fetchFloat(_ca, idx, 0, 1.f);
		v.col[1] = fetchFloat(_ca, idx, 1, 1.f);
		v.col[2] = fetchFloat(_ca, idx, 2, 1.f);
		v.col[3] = fetchFloat(_ca, idx, 3, 1.f);
	} else {
		v.col[0]=_color[0]; v.col[1]=_color[1]; v.col[2]=_color[2]; v.col[3]=_color[3];
	}

	// Texcoord 0 — use current TC if array not enabled
	v.tc0[0] = _ta[0].enabled ? fetchFloat(_ta[0], idx, 0, 0.f) : _currentTC[0][0];
	v.tc0[1] = _ta[0].enabled ? fetchFloat(_ta[0], idx, 1, 0.f) : _currentTC[0][1];
	v.tc0[2] = _ta[0].enabled ? fetchFloat(_ta[0], idx, 2, 0.f) : _currentTC[0][2];
	v.tc0[3] = _ta[0].enabled ? fetchFloat(_ta[0], idx, 3, 1.f) : _currentTC[0][3];

	// Normal — use current normal if array not enabled
	v.norm[0] = _na.enabled ? fetchFloat(_na, idx, 0, 0.f) : _currentNormal[0];
	v.norm[1] = _na.enabled ? fetchFloat(_na, idx, 1, 0.f) : _currentNormal[1];
	v.norm[2] = _na.enabled ? fetchFloat(_na, idx, 2, 1.f) : _currentNormal[2];

	// Texcoord 1 — use current TC if array not enabled
	v.tc1[0] = _ta[1].enabled ? fetchFloat(_ta[1], idx, 0, 0.f) : _currentTC[1][0];
	v.tc1[1] = _ta[1].enabled ? fetchFloat(_ta[1], idx, 1, 0.f) : _currentTC[1][1];
	v.tc1[2] = _ta[1].enabled ? fetchFloat(_ta[1], idx, 2, 0.f) : _currentTC[1][2];
	v.tc1[3] = _ta[1].enabled ? fetchFloat(_ta[1], idx, 3, 1.f) : _currentTC[1][3];
}

void SwitchImmediateRenderer::drawArrays(GLenum mode, GLint first, GLsizei count)
{
	if (count <= 0) return;

	// Translate deprecated primitive types
	bool emitQuads = (mode == GL_QUADS);
	GLenum realMode = mode;
	if (mode == GL_POLYGON) realMode = GL_TRIANGLE_FAN;
	if (mode == GL_QUADS)   realMode = GL_TRIANGLES;

	// Build vertex data
	int nVerts = count;
	// For GL_QUADS: each set of 4 inputs becomes 6 triangle verts
	int outVerts = emitQuads ? (count / 4) * 6 : count;

	ensureVBO(outVerts * sizeof(Vertex));
	std::vector<Vertex> buf;
	buf.resize(outVerts);

	if (emitQuads) {
		int out = 0;
		for (int q = 0; q < count / 4; q++) {
			int base = first + q * 4;
			// Triangle 1: 0,1,2
			fillVertex(buf[out++], base+0);
			fillVertex(buf[out++], base+1);
			fillVertex(buf[out++], base+2);
			// Triangle 2: 0,2,3
			fillVertex(buf[out++], base+0);
			fillVertex(buf[out++], base+2);
			fillVertex(buf[out++], base+3);
		}
	} else {
		for (int i = 0; i < count; i++)
			fillVertex(buf[i], first + i);
	}

	glBindBuffer(GL_ARRAY_BUFFER, _vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, outVerts * sizeof(Vertex), buf.data());

	glBindVertexArray(_vao);

	// Configure attribs on the VAO each time (simple, avoids tracking)
	GLsizei stride = sizeof(Vertex);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, pos));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, col));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, tc0));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, norm));
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, tc1));

	// If no Aleph One shader is active, bind built-in and upload uniforms
	GLint curProg = 0;
	// Use glad's direct function pointer to avoid the macro
	extern void (GLAD_API_PTR *glad_glGetIntegerv)(GLenum, GLint*);
	if (glad_glGetIntegerv) glad_glGetIntegerv(GL_CURRENT_PROGRAM, &curProg);
	bool useBuiltin = (curProg == 0);
	if (useBuiltin) {
		glUseProgram(_builtinProg);
		uploadBuiltinUniforms();
	} else {
		// An Aleph One shader is active. Alpha test state may have changed
		// after Shader::enable() was called (the rendering code typically
		// calls glAlphaFunc/glEnable(GL_ALPHA_TEST) after enabling the
		// shader). Re-upload the current alpha test uniforms so the
		// fragment epilogue's discard logic uses the correct values.
		GLint loc = glGetUniformLocation(curProg, "sw_alphaTestEnabled");
		if (loc >= 0) glUniform1i(loc, (int)g_glState.alphaTestEnabled);
		loc = glGetUniformLocation(curProg, "sw_alphaRef");
		if (loc >= 0) glUniform1f(loc, g_glState.alphaRef);
	}

	// Call glDrawArrays directly via glad pointer
	extern void (GLAD_API_PTR *glad_glDrawArrays)(GLenum, GLint, GLsizei);
	if (glad_glDrawArrays) glad_glDrawArrays(realMode, 0, outVerts);

	if (useBuiltin) glUseProgram(0);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void SwitchImmediateRenderer::drawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
	// GL_TRIANGLES is fine in core; GL_POLYGON cannot appear with indices
	// Just pass through after setting up VAO
	if (count <= 0) return;

	// We still need the VAO/VBO with packed data; figure out max index first
	// For simplicity, scan the index buffer to find the range
	int maxIdx = 0;
	if (type == GL_UNSIGNED_INT) {
		const GLuint *idx = (const GLuint*)indices;
		for (int i = 0; i < count; i++) if ((int)idx[i] > maxIdx) maxIdx = (int)idx[i];
	} else if (type == GL_UNSIGNED_SHORT) {
		const GLushort *idx = (const GLushort*)indices;
		for (int i = 0; i < count; i++) if (idx[i] > maxIdx) maxIdx = idx[i];
	} else if (type == GL_UNSIGNED_BYTE) {
		const GLubyte *idx = (const GLubyte*)indices;
		for (int i = 0; i < count; i++) if (idx[i] > maxIdx) maxIdx = idx[i];
	}
	int nVerts = maxIdx + 1;
	ensureVBO(nVerts * sizeof(Vertex));
	std::vector<Vertex> buf(nVerts);
	for (int i = 0; i < nVerts; i++) fillVertex(buf[i], i);

	glBindBuffer(GL_ARRAY_BUFFER, _vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, nVerts * sizeof(Vertex), buf.data());

	glBindVertexArray(_vao);
	GLsizei stride = sizeof(Vertex);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, pos));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, col));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, tc0));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, norm));
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, tc1));

	GLint curProg = 0;
	extern void (GLAD_API_PTR *glad_glGetIntegerv)(GLenum, GLint*);
	if (glad_glGetIntegerv) glad_glGetIntegerv(GL_CURRENT_PROGRAM, &curProg);
	bool useBuiltin = (curProg == 0);
	if (useBuiltin) {
		glUseProgram(_builtinProg);
		uploadBuiltinUniforms();
	} else {
		// Re-upload alpha test state (may have changed after Shader::enable())
		GLint loc = glGetUniformLocation(curProg, "sw_alphaTestEnabled");
		if (loc >= 0) glUniform1i(loc, (int)g_glState.alphaTestEnabled);
		loc = glGetUniformLocation(curProg, "sw_alphaRef");
		if (loc >= 0) glUniform1f(loc, g_glState.alphaRef);
	}

	// Upload index data to element buffer
	GLuint ebo;
	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	size_t idxBytes = count * (type==GL_UNSIGNED_INT ? 4 : type==GL_UNSIGNED_SHORT ? 2 : 1);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxBytes, indices, GL_STREAM_DRAW);

	extern void (GLAD_API_PTR *glad_glDrawElements)(GLenum, GLsizei, GLenum, const void*);
	if (glad_glDrawElements) glad_glDrawElements(mode, count, type, nullptr);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &ebo);

	if (useBuiltin) glUseProgram(0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Init / Shutdown
// ─────────────────────────────────────────────────────────────────────────────

void SwitchOGL_Init()
{
	g_immediateRenderer.init();
}

#endif // __SWITCH__
