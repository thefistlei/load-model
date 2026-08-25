#ifndef __glad_h_
#define __glad_h_

/* Prevent desktop GL/gl.h from being pulled in after our gl* macros. */
#ifndef __gl_h_
#define __gl_h_
#endif

#include <GLES3/gl3.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*GLADloadproc)(const char *name);

extern int GLAD_GL_VERSION_3_0;

extern PFNGLVIEWPORTPROC glad_glViewport;
extern PFNGLGENTEXTURESPROC glad_glGenTextures;
extern PFNGLBINDTEXTUREPROC glad_glBindTexture;
extern PFNGLTEXIMAGE2DPROC glad_glTexImage2D;
extern PFNGLGENERATEMIPMAPPROC glad_glGenerateMipmap;
extern PFNGLTEXPARAMETERIPROC glad_glTexParameteri;
extern PFNGLACTIVETEXTUREPROC glad_glActiveTexture;
extern PFNGLUNIFORM1IPROC glad_glUniform1i;
extern PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation;
extern PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray;
extern PFNGLDRAWELEMENTSPROC glad_glDrawElements;
extern PFNGLCREATESHADERPROC glad_glCreateShader;
extern PFNGLSHADERSOURCEPROC glad_glShaderSource;
extern PFNGLCOMPILESHADERPROC glad_glCompileShader;
extern PFNGLGETSHADERIVPROC glad_glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog;
extern PFNGLCREATEPROGRAMPROC glad_glCreateProgram;
extern PFNGLATTACHSHADERPROC glad_glAttachShader;
extern PFNGLLINKPROGRAMPROC glad_glLinkProgram;
extern PFNGLDELETESHADERPROC glad_glDeleteShader;
extern PFNGLGETPROGRAMIVPROC glad_glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog;
extern PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays;
extern PFNGLGENBUFFERSPROC glad_glGenBuffers;
extern PFNGLBINDBUFFERPROC glad_glBindBuffer;
extern PFNGLBUFFERDATAPROC glad_glBufferData;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer;
extern PFNGLVERTEXATTRIBIPOINTERPROC glad_glVertexAttribIPointer;
extern PFNGLGETSTRINGPROC glad_glGetString;
extern PFNGLENABLEPROC glad_glEnable;
extern PFNGLCLEARCOLORPROC glad_glClearColor;
extern PFNGLCLEARPROC glad_glClear;
extern PFNGLUSEPROGRAMPROC glad_glUseProgram;
extern PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv;

int gladLoadGLLoader(GLADloadproc load);

#define glViewport glad_glViewport
#define glGenTextures glad_glGenTextures
#define glBindTexture glad_glBindTexture
#define glTexImage2D glad_glTexImage2D
#define glGenerateMipmap glad_glGenerateMipmap
#define glTexParameteri glad_glTexParameteri
#define glActiveTexture glad_glActiveTexture
#define glUniform1i glad_glUniform1i
#define glGetUniformLocation glad_glGetUniformLocation
#define glBindVertexArray glad_glBindVertexArray
#define glDrawElements glad_glDrawElements
#define glCreateShader glad_glCreateShader
#define glShaderSource glad_glShaderSource
#define glCompileShader glad_glCompileShader
#define glGetShaderiv glad_glGetShaderiv
#define glGetShaderInfoLog glad_glGetShaderInfoLog
#define glCreateProgram glad_glCreateProgram
#define glAttachShader glad_glAttachShader
#define glLinkProgram glad_glLinkProgram
#define glDeleteShader glad_glDeleteShader
#define glGetProgramiv glad_glGetProgramiv
#define glGetProgramInfoLog glad_glGetProgramInfoLog
#define glGenVertexArrays glad_glGenVertexArrays
#define glGenBuffers glad_glGenBuffers
#define glBindBuffer glad_glBindBuffer
#define glBufferData glad_glBufferData
#define glEnableVertexAttribArray glad_glEnableVertexAttribArray
#define glVertexAttribPointer glad_glVertexAttribPointer
#define glVertexAttribIPointer glad_glVertexAttribIPointer
#define glGetString glad_glGetString
#define glEnable glad_glEnable
#define glClearColor glad_glClearColor
#define glClear glad_glClear
#define glUseProgram glad_glUseProgram
#define glUniformMatrix4fv glad_glUniformMatrix4fv

#ifdef __cplusplus
}
#endif

#endif
