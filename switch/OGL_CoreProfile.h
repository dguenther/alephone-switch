/*
	OGL_CoreProfile.h — Switch-only shim layer for OpenGL 4.3 Core Profile compatibility.

	Provides:
	  - SwitchMatrixStack : emulates glMatrixMode/glPushMatrix/glTranslate/etc.
	  - SwitchImmediateRenderer : emulates client-state vertex arrays + glDrawArrays
	      (uploads to VAO+VBO each draw, translates GL_POLYGON→GL_TRIANGLE_FAN, GL_QUADS→GL_TRIANGLES)
	  - SwitchGLState : tracks fog, alpha-test, texture-enable state for built-in shader
	  - Macro bridge : #defines that redirect deprecated GL calls to the above

	Include this header from any OGL file that uses the deprecated patterns.
	On non-Switch platforms this header is a no-op.
*/

#pragma once
#ifdef __SWITCH__

#include "OGL_Headers.h"   // brings in glad/glad.h
// Switch glad (0.1.27 modified by fincs) uses APIENTRY/APIENTRYP, not GLAD_API_PTR.
// Define GLAD_API_PTR so OGL_CoreProfile.cpp's local extern declarations compile.
#ifndef GLAD_API_PTR
#define GLAD_API_PTR
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Matrix stack
// ─────────────────────────────────────────────────────────────────────────────

class SwitchMatrixStack {
public:
	SwitchMatrixStack();

	GLenum getMode() const;         // returns current matrix mode
	void matrixMode(GLenum mode);   // GL_MODELVIEW or GL_PROJECTION
	void loadIdentity();
	void pushMatrix();
	void popMatrix();

	void translatef(GLfloat x, GLfloat y, GLfloat z);
	void translated(GLdouble x, GLdouble y, GLdouble z);
	void rotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
	void rotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
	void scalef(GLfloat x, GLfloat y, GLfloat z);
	void scaled(GLdouble x, GLdouble y, GLdouble z);

	// Load/multiply a column-major double matrix
	void loadMatrixd(const GLdouble *m);
	void multMatrixd(const GLdouble *m);
	void loadMatrixf(const GLfloat *m);

	void frustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);
	void ortho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);

	// Read back as doubles/floats (column-major, GL convention)
	void getDoublev(GLenum pname, GLdouble *out) const;
	void getFloatv(GLenum pname, GLfloat *out) const;

	// Accessors for shader upload
	glm::mat4 getMVP()    const;   // projection * modelview
	glm::mat4 getMV()     const;   // modelview
	glm::mat4 getProj()   const;   // projection
	glm::mat3 getNormal() const;   // inverse-transpose of MV 3×3
	glm::mat4 getMVInverse() const;

	// Texture matrix (GL_TEXTURE) — simplified: one level, no stack
	void setTextureMatrix(const glm::mat4 &m);
	glm::mat4 getTextureMatrix() const;

private:
	GLenum _mode;
	std::vector<glm::mat4> _mvStack;
	std::vector<glm::mat4> _projStack;
	glm::mat4 _texMatrix;

	std::vector<glm::mat4>& active();
	const std::vector<glm::mat4>& active() const;
};

extern SwitchMatrixStack g_matrixStack;

// ─────────────────────────────────────────────────────────────────────────────
// GL state (fog, alpha test, texture)
// ─────────────────────────────────────────────────────────────────────────────

struct SwitchGLState {
	// Fog
	bool  fogEnabled  = false;
	GLint fogMode     = 0;         // 0=linear, 1=exp, 2=exp2
	GLfloat fogStart  = 0.0f;
	GLfloat fogEnd    = 1.0f;
	GLfloat fogDensity= 1.0f;
	GLfloat fogColor[4] = {0,0,0,0};

	// Alpha test
	bool  alphaTestEnabled = false;
	GLenum alphaFunc = GL_ALWAYS;
	GLfloat alphaRef = 0.0f;

	// Clip planes (equations stored in eye space)
	glm::vec4 clipPlane[6] = {glm::vec4(0.f), glm::vec4(0.f), glm::vec4(0.f),
	                          glm::vec4(0.f), glm::vec4(0.f), glm::vec4(0.f)};

	// Texture 2D enable
	bool texture2DEnabled = false;

	// Track dirty so built-in shader re-uploads
	bool dirty = true;

	void enable(GLenum cap);
	void disable(GLenum cap);
	void fogf(GLenum pname, GLfloat param);
	void fogfv(GLenum pname, const GLfloat *params);
	void fogi(GLenum pname, GLint param);
	void alphaTestFunc(GLenum func, GLfloat ref);
};

extern SwitchGLState g_glState;

// ─────────────────────────────────────────────────────────────────────────────
// Immediate renderer (replaces client-state vertex arrays)
// ─────────────────────────────────────────────────────────────────────────────

class SwitchImmediateRenderer {
public:
	// Active texture unit for glClientActiveTexture
	enum { MAX_TEX_UNITS = 2 };

	void init();   // call once after glad context is live
	void shutdown();

	// glColor* replacements — set current vertex color
	void color3f(GLfloat r, GLfloat g, GLfloat b)
		{ _color[0]=r; _color[1]=g; _color[2]=b; _color[3]=1.f; }
	void color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
		{ _color[0]=r; _color[1]=g; _color[2]=b; _color[3]=a; }
	void color4fv(const GLfloat *c)
		{ _color[0]=c[0]; _color[1]=c[1]; _color[2]=c[2]; _color[3]=c[3]; }
	void color3fv(const GLfloat *c)
		{ _color[0]=c[0]; _color[1]=c[1]; _color[2]=c[2]; _color[3]=1.f; }
	void color4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)
		{ _color[0]=r/255.f; _color[1]=g/255.f; _color[2]=b/255.f; _color[3]=a/255.f; }
	void color3ub(GLubyte r, GLubyte g, GLubyte b)
		{ _color[0]=r/255.f; _color[1]=g/255.f; _color[2]=b/255.f; _color[3]=1.f; }
	void color3us(GLushort r, GLushort g, GLushort b)
		{ _color[0]=r/65535.f; _color[1]=g/65535.f; _color[2]=b/65535.f; _color[3]=1.f; }
	void color3usv(const GLushort *c)
		{ _color[0]=c[0]/65535.f; _color[1]=c[1]/65535.f; _color[2]=c[2]/65535.f; _color[3]=1.f; }
	void color4usv(const GLushort *c)
		{ _color[0]=c[0]/65535.f; _color[1]=c[1]/65535.f; _color[2]=c[2]/65535.f; _color[3]=c[3]/65535.f; }
	void color4us(GLushort r, GLushort g, GLushort b, GLushort a)
		{ _color[0]=r/65535.f; _color[1]=g/65535.f; _color[2]=b/65535.f; _color[3]=a/65535.f; }

	// glNormal3f — set current normal (used when normal array not enabled)
	void normal3f(GLfloat x, GLfloat y, GLfloat z)
		{ _currentNormal[0]=x; _currentNormal[1]=y; _currentNormal[2]=z; }

	// glMultiTexCoord — set current texcoord for specified unit
	void multiTexCoord4f(GLenum unit, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
		{ int u=(unit==GL_TEXTURE1)?1:0; _currentTC[u][0]=s; _currentTC[u][1]=t; _currentTC[u][2]=r; _currentTC[u][3]=q; }
	void multiTexCoord2f(GLenum unit, GLfloat s, GLfloat t)
		{ multiTexCoord4f(unit, s, t, 0.f, 1.f); }

	// glClientActiveTexture
	void clientActiveTexture(GLenum unit);  // GL_TEXTURE0 or GL_TEXTURE1

	// glEnableClientState / glDisableClientState
	void enableClientState(GLenum cap);
	void disableClientState(GLenum cap);

	// Pointer setters (store but don't upload yet)
	void vertexPointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
	void texCoordPointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
	void colorPointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
	void normalPointer(GLenum type, GLsizei stride, const void *ptr);

	// Draw calls — pack client data → VBO, configure VAO, draw
	void drawArrays(GLenum mode, GLint first, GLsizei count);
	void drawElements(GLenum mode, GLsizei count, GLenum type, const void *indices);

	// Built-in shader program management
	void initBuiltinShader();
	void uploadBuiltinUniforms();

private:
	struct ArrayState {
		GLint    size   = 4;
		GLenum   type   = GL_FLOAT;
		GLsizei  stride = 0;
		const void *ptr = nullptr;
		bool     enabled = false;
	};

	ArrayState _va;                      // vertex (position)
	ArrayState _ta[MAX_TEX_UNITS];       // texcoords 0,1
	ArrayState _ca;                      // color
	ArrayState _na;                      // normal

	GLint  _activeTexUnit = 0;           // index for glClientActiveTexture
	GLfloat _color[4] = {1,1,1,1};
	GLfloat _currentNormal[3] = {0,0,1};
	GLfloat _currentTC[MAX_TEX_UNITS][4] = {{0,0,0,1},{0,0,0,1}};

	GLuint _vao = 0;
	GLuint _vbo = 0;
	size_t _vboCapacity = 0;

	// Built-in fallback shader (used when no Aleph One shader is enabled)
	GLuint _builtinProg = 0;
	GLint  _uMVP = -1, _uMV = -1, _uTexMat = -1;
	GLint  _uFogEnabled=-1, _uFogMode=-1, _uFogStart=-1, _uFogEnd=-1, _uFogDensity=-1;
	GLint  _uFogColor=-1;
	GLint  _uAlphaEnabled=-1, _uAlphaRef=-1;
	GLint  _uTexEnabled=-1;
	GLint  _uTex0=-1;
	GLint  _uClipPlane[6] = {-1,-1,-1,-1,-1,-1};

	// Interleaved vertex layout (must match built-in shader attribute locations)
	struct Vertex {
		GLfloat pos[4];    // loc 0
		GLfloat col[4];    // loc 1
		GLfloat tc0[4];    // loc 2
		GLfloat norm[3];   // loc 3
		GLfloat tc1[4];    // loc 4
	};

	void ensureVBO(size_t bytes);
	GLfloat fetchFloat(const ArrayState &a, int idx, int component, GLfloat def) const;
	void fillVertex(Vertex &v, int idx) const;
	GLuint compileBuiltinShader(GLenum type, const char *src);
};

extern SwitchImmediateRenderer g_immediateRenderer;

// ─────────────────────────────────────────────────────────────────────────────
// Initialise / teardown — call from screen.cpp context setup
// ─────────────────────────────────────────────────────────────────────────────

void SwitchOGL_Init();    // call once after gladLoadGL()

// ─────────────────────────────────────────────────────────────────────────────
// Macro bridge — silently reroute deprecated GL calls to the shim
// ─────────────────────────────────────────────────────────────────────────────

// Matrix stack
#define glMatrixMode(m)          g_matrixStack.matrixMode(m)
#define glLoadIdentity()         g_matrixStack.loadIdentity()
#define glPushMatrix()           g_matrixStack.pushMatrix()
#define glPopMatrix()            g_matrixStack.popMatrix()
#define glTranslatef(x,y,z)      g_matrixStack.translatef(x,y,z)
#define glTranslated(x,y,z)      g_matrixStack.translated(x,y,z)
#define glRotatef(a,x,y,z)       g_matrixStack.rotatef(a,x,y,z)
#define glRotated(a,x,y,z)       g_matrixStack.rotated(a,x,y,z)
#define glScalef(x,y,z)          g_matrixStack.scalef(x,y,z)
#define glScaled(x,y,z)          g_matrixStack.scaled(x,y,z)
#define glLoadMatrixd(m)         g_matrixStack.loadMatrixd(m)
#define glLoadMatrixf(m)         g_matrixStack.loadMatrixf(m)
#define glMultMatrixd(m)         g_matrixStack.multMatrixd(m)
#define glFrustum(l,r,b,t,n,f)  g_matrixStack.frustum(l,r,b,t,n,f)
#define glOrtho(l,r,b,t,n,f)    g_matrixStack.ortho(l,r,b,t,n,f)
// glGetDoublev / glGetFloatv: only intercept matrix queries; others pass through
// (We use a wrapper function to avoid breaking non-matrix queries)
void SwitchGetDoublev(GLenum pname, GLdouble *params);
void SwitchGetFloatv(GLenum pname, GLfloat *params);
void SwitchGetIntegerv(GLenum pname, GLint *params);
// Deprecated GL constants not in core profile headers
#ifndef GL_MATRIX_MODE
#define GL_MATRIX_MODE         0x0BA0
#endif
#ifndef GL_MODELVIEW
#define GL_MODELVIEW           0x1700
#endif
#ifndef GL_PROJECTION
#define GL_PROJECTION          0x1701
#endif
#ifndef GL_TEXTURE
#define GL_TEXTURE             0x1702
#endif
#ifndef GL_VERTEX_ARRAY
#define GL_VERTEX_ARRAY        0x8074
#endif
#ifndef GL_NORMAL_ARRAY
#define GL_NORMAL_ARRAY        0x8075
#endif
#ifndef GL_COLOR_ARRAY
#define GL_COLOR_ARRAY         0x8076
#endif
#ifndef GL_TEXTURE_COORD_ARRAY
#define GL_TEXTURE_COORD_ARRAY 0x8078
#endif
#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST          0x0BC0
#endif
#ifndef GL_FOG
#define GL_FOG                 0x0B60
#endif
#ifndef GL_POLYGON
#define GL_POLYGON             0x0009
#endif
#ifndef GL_QUADS
#define GL_QUADS               0x0007
#endif
#ifndef GL_QUAD_STRIP
#define GL_QUAD_STRIP          0x0008
#endif
// GL_FRAMEBUFFER_sRGB (old EXT-style spelling) — same value as GL_FRAMEBUFFER_SRGB
#ifndef GL_FRAMEBUFFER_sRGB
#define GL_FRAMEBUFFER_sRGB    GL_FRAMEBUFFER_SRGB
#endif
// Fog parameter constants (deprecated in core)
#ifndef GL_FOG_DENSITY
#define GL_FOG_DENSITY   0x0B62
#endif
#ifndef GL_FOG_START
#define GL_FOG_START     0x0B63
#endif
#ifndef GL_FOG_END
#define GL_FOG_END       0x0B64
#endif
#ifndef GL_FOG_MODE
#define GL_FOG_MODE      0x0B65
#endif
#ifndef GL_FOG_COLOR
#define GL_FOG_COLOR     0x0B66
#endif
#ifndef GL_EXP
#define GL_EXP           0x0800
#endif
#ifndef GL_EXP2
#define GL_EXP2          0x0801
#endif
// Alpha test constants (deprecated in core)
#ifndef GL_ALWAYS
#define GL_ALWAYS        0x0207
#endif
// Matrix query constants (deprecated in core)
#ifndef GL_MODELVIEW_MATRIX
#define GL_MODELVIEW_MATRIX  0x0BA6
#endif
#ifndef GL_PROJECTION_MATRIX
#define GL_PROJECTION_MATRIX 0x0BA7
#endif
#ifndef GL_TEXTURE_MATRIX
#define GL_TEXTURE_MATRIX    0x0BA8
#endif
// GL_CLIP_DISTANCEn — may not be in Switch glad headers
#ifndef GL_CLIP_DISTANCE0
#define GL_CLIP_DISTANCE0  0x3000
#define GL_CLIP_DISTANCE1  0x3001
#define GL_CLIP_DISTANCE2  0x3002
#define GL_CLIP_DISTANCE3  0x3003
#define GL_CLIP_DISTANCE4  0x3004
#define GL_CLIP_DISTANCE5  0x3005
#endif
// GL_CLIP_PLANEn → GL_CLIP_DISTANCEn (all no-oped via SwitchEnable/SwitchDisable)
#ifndef GL_CLIP_PLANE0
#define GL_CLIP_PLANE0  GL_CLIP_DISTANCE0
#define GL_CLIP_PLANE1  GL_CLIP_DISTANCE1
#define GL_CLIP_PLANE2  GL_CLIP_DISTANCE2
#define GL_CLIP_PLANE3  GL_CLIP_DISTANCE3
#define GL_CLIP_PLANE4  GL_CLIP_DISTANCE4
#define GL_CLIP_PLANE5  GL_CLIP_DISTANCE5
#endif
// GL_CLAMP removed in GL 3.1; use GL_CLAMP_TO_EDGE as substitute
#ifndef GL_CLAMP
#define GL_CLAMP        GL_CLAMP_TO_EDGE
#endif
#undef glGetDoublev
#define glGetDoublev(p,v)        SwitchGetDoublev(p,v)
#undef glGetFloatv
#define glGetFloatv(p,v)         SwitchGetFloatv(p,v)
#undef glGetIntegerv
#define glGetIntegerv(p,v)       SwitchGetIntegerv(p,v)

// Color
#define glColor3f(r,g,b)         g_immediateRenderer.color3f(r,g,b)
#define glColor4f(r,g,b,a)       g_immediateRenderer.color4f(r,g,b,a)
#define glColor4fv(c)            g_immediateRenderer.color4fv(c)
#define glColor3fv(c)            g_immediateRenderer.color3fv(c)
#define glColor4ub(r,g,b,a)      g_immediateRenderer.color4ub(r,g,b,a)
#define glColor3ub(r,g,b)        g_immediateRenderer.color3ub(r,g,b)
#define glColor3us(r,g,b)        g_immediateRenderer.color3us(r,g,b)
#define glColor3usv(c)           g_immediateRenderer.color3usv(c)
#define glColor4usv(c)           g_immediateRenderer.color4usv(c)
#define glColor4us(r,g,b,a)      g_immediateRenderer.color4us(r,g,b,a)

// Active texture unit (server-side)
#define glActiveTextureARB(u)            glActiveTexture(u)
#define GL_TEXTURE0_ARB                  GL_TEXTURE0
#define GL_TEXTURE1_ARB                  GL_TEXTURE1
// GL_TEXTURE_RECTANGLE_ARB → GL_TEXTURE_2D (use normalized tex coords)
#define GL_TEXTURE_RECTANGLE_ARB         GL_TEXTURE_2D

// Immediate-mode normal (used as per-draw-call constant when normal array not enabled)
#define glNormal3f(x,y,z)               g_immediateRenderer.normal3f(x,y,z)
// glMultiTexCoord — set current texcoord for multi-texturing
#define glMultiTexCoord4fARB(u,s,t,r,q) g_immediateRenderer.multiTexCoord4f(u,s,t,r,q)
#define glMultiTexCoord2fARB(u,s,t)     g_immediateRenderer.multiTexCoord2f(u,s,t)
#define glMultiTexCoord4f(u,s,t,r,q)    g_immediateRenderer.multiTexCoord4f(u,s,t,r,q)
#define glMultiTexCoord2f(u,s,t)        g_immediateRenderer.multiTexCoord2f(u,s,t)

// Client state / vertex arrays
#define glClientActiveTexture(u)         g_immediateRenderer.clientActiveTexture(u)
#define glClientActiveTextureARB(u)      g_immediateRenderer.clientActiveTexture(u)
#define glEnableClientState(c)           g_immediateRenderer.enableClientState(c)
#define glDisableClientState(c)          g_immediateRenderer.disableClientState(c)
#define glVertexPointer(s,t,st,p)        g_immediateRenderer.vertexPointer(s,t,st,p)
#define glTexCoordPointer(s,t,st,p)      g_immediateRenderer.texCoordPointer(s,t,st,p)
#define glColorPointer(s,t,st,p)         g_immediateRenderer.colorPointer(s,t,st,p)
#define glNormalPointer(t,st,p)          g_immediateRenderer.normalPointer(t,st,p)

// Draw — reroute so GL_POLYGON / GL_QUADS get translated
#undef glDrawArrays
#define glDrawArrays(m,f,c)      g_immediateRenderer.drawArrays(m,f,c)
#undef glDrawElements
#define glDrawElements(m,c,t,i)  g_immediateRenderer.drawElements(m,c,t,i)

// GL state (fog, alpha test, texture)
void SwitchEnable(GLenum cap);
void SwitchDisable(GLenum cap);
void SwitchClipPlane(GLenum plane, const GLdouble *equation);
#undef glEnable
#define glEnable(c)              SwitchEnable(c)
#undef glDisable
#define glDisable(c)             SwitchDisable(c)

#define glFogf(p,v)              g_glState.fogf(p,v)
#define glFogfv(p,v)             g_glState.fogfv(p,v)
#define glFogi(p,v)              g_glState.fogi(p,v)
#define glAlphaFunc(f,r)         g_glState.alphaTestFunc(f,r)

// Deprecated texture environment — no-op in core (shaders handle it)
#define glTexEnvi(t,p,v)         ((void)0)
#define glTexEnvf(t,p,v)         ((void)0)
#define glTexEnvfv(t,p,v)        ((void)0)

// Display lists — stubbed (FontHandler and OGL_Render have __SWITCH__ paths)
#define glGenLists(n)            (0)
#define glNewList(l,m)           ((void)0)
#define glEndList()              ((void)0)
#define glCallList(l)            ((void)0)
#define glListBase(b)            ((void)0)
#define glDeleteLists(l,n)       ((void)0)

// Logic op — no-op (faders use UseFlatStatic fallback on Switch)
#undef glLogicOp
#define glLogicOp(op)            ((void)0)

// Push/pop attrib — no-op (per-file code will save/restore manually where needed)
#define glPushAttrib(mask)       ((void)0)
#define glPopAttrib()            ((void)0)

// Clip planes — transform equation to eye space and store; shader epilogue computes gl_ClipDistance
#define glClipPlane(p,e)         SwitchClipPlane(p,e)

// sRGB framebuffer — EXT → core rename
#ifndef GL_FRAMEBUFFER_SRGB_EXT
#define GL_FRAMEBUFFER_SRGB_EXT  GL_FRAMEBUFFER_SRGB
#endif

// Compressed textures — ARB → core rename
#define glCompressedTexImage2DARB glCompressedTexImage2D
#ifndef GL_ARB_texture_compression
#define GL_ARB_texture_compression 1
#endif

// Old glGet* for extension strings — handled differently in core
// (OGL_Setup.cpp has its own __SWITCH__ branch)

#endif // __SWITCH__
