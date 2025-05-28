bits 64

; For some reason libGL also exposes GLX symbols so we need them here
; This means if this gets loaded first it will also run the GLX constructor which we are fine with
%include "libGLX.asm"

section .text

global glAccum:function
align 16
glAccum:
invlpg [rax]
db "glAccum", 0
ret

global glAlphaFunc:function
align 16
glAlphaFunc:
invlpg [rax]
db "glAlphaFunc", 0
ret

global glBegin:function
align 16
glBegin:
invlpg [rax]
db "glBegin", 0
ret

global glBitmap:function
align 16
glBitmap:
invlpg [rax]
db "glBitmap", 0
ret

global glBlendFunc:function
align 16
glBlendFunc:
invlpg [rax]
db "glBlendFunc", 0
ret

global glCallList:function
align 16
glCallList:
invlpg [rax]
db "glCallList", 0
ret

global glCallLists:function
align 16
glCallLists:
invlpg [rax]
db "glCallLists", 0
ret

global glClear:function
align 16
glClear:
invlpg [rax]
db "glClear", 0
ret

global glClearAccum:function
align 16
glClearAccum:
invlpg [rax]
db "glClearAccum", 0
ret

global glClearColor:function
align 16
glClearColor:
invlpg [rax]
db "glClearColor", 0
ret

global glClearDepth:function
align 16
glClearDepth:
invlpg [rax]
db "glClearDepth", 0
ret

global glClearIndex:function
align 16
glClearIndex:
invlpg [rax]
db "glClearIndex", 0
ret

global glClearStencil:function
align 16
glClearStencil:
invlpg [rax]
db "glClearStencil", 0
ret

global glClipPlane:function
align 16
glClipPlane:
invlpg [rax]
db "glClipPlane", 0
ret

global glColor3b:function
align 16
glColor3b:
invlpg [rax]
db "glColor3b", 0
ret

global glColor3bv:function
align 16
glColor3bv:
invlpg [rax]
db "glColor3bv", 0
ret

global glColor3d:function
align 16
glColor3d:
invlpg [rax]
db "glColor3d", 0
ret

global glColor3dv:function
align 16
glColor3dv:
invlpg [rax]
db "glColor3dv", 0
ret

global glColor3f:function
align 16
glColor3f:
invlpg [rax]
db "glColor3f", 0
ret

global glColor3fv:function
align 16
glColor3fv:
invlpg [rax]
db "glColor3fv", 0
ret

global glColor3i:function
align 16
glColor3i:
invlpg [rax]
db "glColor3i", 0
ret

global glColor3iv:function
align 16
glColor3iv:
invlpg [rax]
db "glColor3iv", 0
ret

global glColor3s:function
align 16
glColor3s:
invlpg [rax]
db "glColor3s", 0
ret

global glColor3sv:function
align 16
glColor3sv:
invlpg [rax]
db "glColor3sv", 0
ret

global glColor3ub:function
align 16
glColor3ub:
invlpg [rax]
db "glColor3ub", 0
ret

global glColor3ubv:function
align 16
glColor3ubv:
invlpg [rax]
db "glColor3ubv", 0
ret

global glColor3ui:function
align 16
glColor3ui:
invlpg [rax]
db "glColor3ui", 0
ret

global glColor3uiv:function
align 16
glColor3uiv:
invlpg [rax]
db "glColor3uiv", 0
ret

global glColor3us:function
align 16
glColor3us:
invlpg [rax]
db "glColor3us", 0
ret

global glColor3usv:function
align 16
glColor3usv:
invlpg [rax]
db "glColor3usv", 0
ret

global glColor4b:function
align 16
glColor4b:
invlpg [rax]
db "glColor4b", 0
ret

global glColor4bv:function
align 16
glColor4bv:
invlpg [rax]
db "glColor4bv", 0
ret

global glColor4d:function
align 16
glColor4d:
invlpg [rax]
db "glColor4d", 0
ret

global glColor4dv:function
align 16
glColor4dv:
invlpg [rax]
db "glColor4dv", 0
ret

global glColor4f:function
align 16
glColor4f:
invlpg [rax]
db "glColor4f", 0
ret

global glColor4fv:function
align 16
glColor4fv:
invlpg [rax]
db "glColor4fv", 0
ret

global glColor4i:function
align 16
glColor4i:
invlpg [rax]
db "glColor4i", 0
ret

global glColor4iv:function
align 16
glColor4iv:
invlpg [rax]
db "glColor4iv", 0
ret

global glColor4s:function
align 16
glColor4s:
invlpg [rax]
db "glColor4s", 0
ret

global glColor4sv:function
align 16
glColor4sv:
invlpg [rax]
db "glColor4sv", 0
ret

global glColor4ub:function
align 16
glColor4ub:
invlpg [rax]
db "glColor4ub", 0
ret

global glColor4ubv:function
align 16
glColor4ubv:
invlpg [rax]
db "glColor4ubv", 0
ret

global glColor4ui:function
align 16
glColor4ui:
invlpg [rax]
db "glColor4ui", 0
ret

global glColor4uiv:function
align 16
glColor4uiv:
invlpg [rax]
db "glColor4uiv", 0
ret

global glColor4us:function
align 16
glColor4us:
invlpg [rax]
db "glColor4us", 0
ret

global glColor4usv:function
align 16
glColor4usv:
invlpg [rax]
db "glColor4usv", 0
ret

global glColorMask:function
align 16
glColorMask:
invlpg [rax]
db "glColorMask", 0
ret

global glColorMaterial:function
align 16
glColorMaterial:
invlpg [rax]
db "glColorMaterial", 0
ret

global glCopyPixels:function
align 16
glCopyPixels:
invlpg [rax]
db "glCopyPixels", 0
ret

global glCullFace:function
align 16
glCullFace:
invlpg [rax]
db "glCullFace", 0
ret

global glDeleteLists:function
align 16
glDeleteLists:
invlpg [rax]
db "glDeleteLists", 0
ret

global glDepthFunc:function
align 16
glDepthFunc:
invlpg [rax]
db "glDepthFunc", 0
ret

global glDepthMask:function
align 16
glDepthMask:
invlpg [rax]
db "glDepthMask", 0
ret

global glDepthRange:function
align 16
glDepthRange:
invlpg [rax]
db "glDepthRange", 0
ret

global glDisable:function
align 16
glDisable:
invlpg [rax]
db "glDisable", 0
ret

global glDrawBuffer:function
align 16
glDrawBuffer:
invlpg [rax]
db "glDrawBuffer", 0
ret

global glDrawPixels:function
align 16
glDrawPixels:
invlpg [rax]
db "glDrawPixels", 0
ret

global glEdgeFlag:function
align 16
glEdgeFlag:
invlpg [rax]
db "glEdgeFlag", 0
ret

global glEdgeFlagv:function
align 16
glEdgeFlagv:
invlpg [rax]
db "glEdgeFlagv", 0
ret

global glEnable:function
align 16
glEnable:
invlpg [rax]
db "glEnable", 0
ret

global glEnd:function
align 16
glEnd:
invlpg [rax]
db "glEnd", 0
ret

global glEndList:function
align 16
glEndList:
invlpg [rax]
db "glEndList", 0
ret

global glEvalCoord1d:function
align 16
glEvalCoord1d:
invlpg [rax]
db "glEvalCoord1d", 0
ret

global glEvalCoord1dv:function
align 16
glEvalCoord1dv:
invlpg [rax]
db "glEvalCoord1dv", 0
ret

global glEvalCoord1f:function
align 16
glEvalCoord1f:
invlpg [rax]
db "glEvalCoord1f", 0
ret

global glEvalCoord1fv:function
align 16
glEvalCoord1fv:
invlpg [rax]
db "glEvalCoord1fv", 0
ret

global glEvalCoord2d:function
align 16
glEvalCoord2d:
invlpg [rax]
db "glEvalCoord2d", 0
ret

global glEvalCoord2dv:function
align 16
glEvalCoord2dv:
invlpg [rax]
db "glEvalCoord2dv", 0
ret

global glEvalCoord2f:function
align 16
glEvalCoord2f:
invlpg [rax]
db "glEvalCoord2f", 0
ret

global glEvalCoord2fv:function
align 16
glEvalCoord2fv:
invlpg [rax]
db "glEvalCoord2fv", 0
ret

global glEvalMesh1:function
align 16
glEvalMesh1:
invlpg [rax]
db "glEvalMesh1", 0
ret

global glEvalMesh2:function
align 16
glEvalMesh2:
invlpg [rax]
db "glEvalMesh2", 0
ret

global glEvalPoint1:function
align 16
glEvalPoint1:
invlpg [rax]
db "glEvalPoint1", 0
ret

global glEvalPoint2:function
align 16
glEvalPoint2:
invlpg [rax]
db "glEvalPoint2", 0
ret

global glFeedbackBuffer:function
align 16
glFeedbackBuffer:
invlpg [rax]
db "glFeedbackBuffer", 0
ret

global glFinish:function
align 16
glFinish:
invlpg [rax]
db "glFinish", 0
ret

global glFlush:function
align 16
glFlush:
invlpg [rax]
db "glFlush", 0
ret

global glFogf:function
align 16
glFogf:
invlpg [rax]
db "glFogf", 0
ret

global glFogfv:function
align 16
glFogfv:
invlpg [rax]
db "glFogfv", 0
ret

global glFogi:function
align 16
glFogi:
invlpg [rax]
db "glFogi", 0
ret

global glFogiv:function
align 16
glFogiv:
invlpg [rax]
db "glFogiv", 0
ret

global glFrontFace:function
align 16
glFrontFace:
invlpg [rax]
db "glFrontFace", 0
ret

global glFrustum:function
align 16
glFrustum:
invlpg [rax]
db "glFrustum", 0
ret

global glGenLists:function
align 16
glGenLists:
invlpg [rax]
db "glGenLists", 0
ret

global glGetBooleanv:function
align 16
glGetBooleanv:
invlpg [rax]
db "glGetBooleanv", 0
ret

global glGetClipPlane:function
align 16
glGetClipPlane:
invlpg [rax]
db "glGetClipPlane", 0
ret

global glGetDoublev:function
align 16
glGetDoublev:
invlpg [rax]
db "glGetDoublev", 0
ret

global glGetError:function
align 16
glGetError:
invlpg [rax]
db "glGetError", 0
ret

global glGetFloatv:function
align 16
glGetFloatv:
invlpg [rax]
db "glGetFloatv", 0
ret

global glGetIntegerv:function
align 16
glGetIntegerv:
invlpg [rax]
db "glGetIntegerv", 0
ret

global glGetLightfv:function
align 16
glGetLightfv:
invlpg [rax]
db "glGetLightfv", 0
ret

global glGetLightiv:function
align 16
glGetLightiv:
invlpg [rax]
db "glGetLightiv", 0
ret

global glGetMapdv:function
align 16
glGetMapdv:
invlpg [rax]
db "glGetMapdv", 0
ret

global glGetMapfv:function
align 16
glGetMapfv:
invlpg [rax]
db "glGetMapfv", 0
ret

global glGetMapiv:function
align 16
glGetMapiv:
invlpg [rax]
db "glGetMapiv", 0
ret

global glGetMaterialfv:function
align 16
glGetMaterialfv:
invlpg [rax]
db "glGetMaterialfv", 0
ret

global glGetMaterialiv:function
align 16
glGetMaterialiv:
invlpg [rax]
db "glGetMaterialiv", 0
ret

global glGetPixelMapfv:function
align 16
glGetPixelMapfv:
invlpg [rax]
db "glGetPixelMapfv", 0
ret

global glGetPixelMapuiv:function
align 16
glGetPixelMapuiv:
invlpg [rax]
db "glGetPixelMapuiv", 0
ret

global glGetPixelMapusv:function
align 16
glGetPixelMapusv:
invlpg [rax]
db "glGetPixelMapusv", 0
ret

global glGetPolygonStipple:function
align 16
glGetPolygonStipple:
invlpg [rax]
db "glGetPolygonStipple", 0
ret

global glGetString:function
align 16
glGetString:
invlpg [rax]
db "glGetString", 0
ret

global glGetTexEnvfv:function
align 16
glGetTexEnvfv:
invlpg [rax]
db "glGetTexEnvfv", 0
ret

global glGetTexEnviv:function
align 16
glGetTexEnviv:
invlpg [rax]
db "glGetTexEnviv", 0
ret

global glGetTexGendv:function
align 16
glGetTexGendv:
invlpg [rax]
db "glGetTexGendv", 0
ret

global glGetTexGenfv:function
align 16
glGetTexGenfv:
invlpg [rax]
db "glGetTexGenfv", 0
ret

global glGetTexGeniv:function
align 16
glGetTexGeniv:
invlpg [rax]
db "glGetTexGeniv", 0
ret

global glGetTexImage:function
align 16
glGetTexImage:
invlpg [rax]
db "glGetTexImage", 0
ret

global glGetTexLevelParameterfv:function
align 16
glGetTexLevelParameterfv:
invlpg [rax]
db "glGetTexLevelParameterfv", 0
ret

global glGetTexLevelParameteriv:function
align 16
glGetTexLevelParameteriv:
invlpg [rax]
db "glGetTexLevelParameteriv", 0
ret

global glGetTexParameterfv:function
align 16
glGetTexParameterfv:
invlpg [rax]
db "glGetTexParameterfv", 0
ret

global glGetTexParameteriv:function
align 16
glGetTexParameteriv:
invlpg [rax]
db "glGetTexParameteriv", 0
ret

global glHint:function
align 16
glHint:
invlpg [rax]
db "glHint", 0
ret

global glIndexMask:function
align 16
glIndexMask:
invlpg [rax]
db "glIndexMask", 0
ret

global glIndexd:function
align 16
glIndexd:
invlpg [rax]
db "glIndexd", 0
ret

global glIndexdv:function
align 16
glIndexdv:
invlpg [rax]
db "glIndexdv", 0
ret

global glIndexf:function
align 16
glIndexf:
invlpg [rax]
db "glIndexf", 0
ret

global glIndexfv:function
align 16
glIndexfv:
invlpg [rax]
db "glIndexfv", 0
ret

global glIndexi:function
align 16
glIndexi:
invlpg [rax]
db "glIndexi", 0
ret

global glIndexiv:function
align 16
glIndexiv:
invlpg [rax]
db "glIndexiv", 0
ret

global glIndexs:function
align 16
glIndexs:
invlpg [rax]
db "glIndexs", 0
ret

global glIndexsv:function
align 16
glIndexsv:
invlpg [rax]
db "glIndexsv", 0
ret

global glInitNames:function
align 16
glInitNames:
invlpg [rax]
db "glInitNames", 0
ret

global glIsEnabled:function
align 16
glIsEnabled:
invlpg [rax]
db "glIsEnabled", 0
ret

global glIsList:function
align 16
glIsList:
invlpg [rax]
db "glIsList", 0
ret

global glLightModelf:function
align 16
glLightModelf:
invlpg [rax]
db "glLightModelf", 0
ret

global glLightModelfv:function
align 16
glLightModelfv:
invlpg [rax]
db "glLightModelfv", 0
ret

global glLightModeli:function
align 16
glLightModeli:
invlpg [rax]
db "glLightModeli", 0
ret

global glLightModeliv:function
align 16
glLightModeliv:
invlpg [rax]
db "glLightModeliv", 0
ret

global glLightf:function
align 16
glLightf:
invlpg [rax]
db "glLightf", 0
ret

global glLightfv:function
align 16
glLightfv:
invlpg [rax]
db "glLightfv", 0
ret

global glLighti:function
align 16
glLighti:
invlpg [rax]
db "glLighti", 0
ret

global glLightiv:function
align 16
glLightiv:
invlpg [rax]
db "glLightiv", 0
ret

global glLineStipple:function
align 16
glLineStipple:
invlpg [rax]
db "glLineStipple", 0
ret

global glLineWidth:function
align 16
glLineWidth:
invlpg [rax]
db "glLineWidth", 0
ret

global glListBase:function
align 16
glListBase:
invlpg [rax]
db "glListBase", 0
ret

global glLoadIdentity:function
align 16
glLoadIdentity:
invlpg [rax]
db "glLoadIdentity", 0
ret

global glLoadMatrixd:function
align 16
glLoadMatrixd:
invlpg [rax]
db "glLoadMatrixd", 0
ret

global glLoadMatrixf:function
align 16
glLoadMatrixf:
invlpg [rax]
db "glLoadMatrixf", 0
ret

global glLoadName:function
align 16
glLoadName:
invlpg [rax]
db "glLoadName", 0
ret

global glLogicOp:function
align 16
glLogicOp:
invlpg [rax]
db "glLogicOp", 0
ret

global glMap1d:function
align 16
glMap1d:
invlpg [rax]
db "glMap1d", 0
ret

global glMap1f:function
align 16
glMap1f:
invlpg [rax]
db "glMap1f", 0
ret

global glMap2d:function
align 16
glMap2d:
invlpg [rax]
db "glMap2d", 0
ret

global glMap2f:function
align 16
glMap2f:
invlpg [rax]
db "glMap2f", 0
ret

global glMapGrid1d:function
align 16
glMapGrid1d:
invlpg [rax]
db "glMapGrid1d", 0
ret

global glMapGrid1f:function
align 16
glMapGrid1f:
invlpg [rax]
db "glMapGrid1f", 0
ret

global glMapGrid2d:function
align 16
glMapGrid2d:
invlpg [rax]
db "glMapGrid2d", 0
ret

global glMapGrid2f:function
align 16
glMapGrid2f:
invlpg [rax]
db "glMapGrid2f", 0
ret

global glMaterialf:function
align 16
glMaterialf:
invlpg [rax]
db "glMaterialf", 0
ret

global glMaterialfv:function
align 16
glMaterialfv:
invlpg [rax]
db "glMaterialfv", 0
ret

global glMateriali:function
align 16
glMateriali:
invlpg [rax]
db "glMateriali", 0
ret

global glMaterialiv:function
align 16
glMaterialiv:
invlpg [rax]
db "glMaterialiv", 0
ret

global glMatrixMode:function
align 16
glMatrixMode:
invlpg [rax]
db "glMatrixMode", 0
ret

global glMultMatrixd:function
align 16
glMultMatrixd:
invlpg [rax]
db "glMultMatrixd", 0
ret

global glMultMatrixf:function
align 16
glMultMatrixf:
invlpg [rax]
db "glMultMatrixf", 0
ret

global glNewList:function
align 16
glNewList:
invlpg [rax]
db "glNewList", 0
ret

global glNormal3b:function
align 16
glNormal3b:
invlpg [rax]
db "glNormal3b", 0
ret

global glNormal3bv:function
align 16
glNormal3bv:
invlpg [rax]
db "glNormal3bv", 0
ret

global glNormal3d:function
align 16
glNormal3d:
invlpg [rax]
db "glNormal3d", 0
ret

global glNormal3dv:function
align 16
glNormal3dv:
invlpg [rax]
db "glNormal3dv", 0
ret

global glNormal3f:function
align 16
glNormal3f:
invlpg [rax]
db "glNormal3f", 0
ret

global glNormal3fv:function
align 16
glNormal3fv:
invlpg [rax]
db "glNormal3fv", 0
ret

global glNormal3i:function
align 16
glNormal3i:
invlpg [rax]
db "glNormal3i", 0
ret

global glNormal3iv:function
align 16
glNormal3iv:
invlpg [rax]
db "glNormal3iv", 0
ret

global glNormal3s:function
align 16
glNormal3s:
invlpg [rax]
db "glNormal3s", 0
ret

global glNormal3sv:function
align 16
glNormal3sv:
invlpg [rax]
db "glNormal3sv", 0
ret

global glOrtho:function
align 16
glOrtho:
invlpg [rax]
db "glOrtho", 0
ret

global glPassThrough:function
align 16
glPassThrough:
invlpg [rax]
db "glPassThrough", 0
ret

global glPixelMapfv:function
align 16
glPixelMapfv:
invlpg [rax]
db "glPixelMapfv", 0
ret

global glPixelMapuiv:function
align 16
glPixelMapuiv:
invlpg [rax]
db "glPixelMapuiv", 0
ret

global glPixelMapusv:function
align 16
glPixelMapusv:
invlpg [rax]
db "glPixelMapusv", 0
ret

global glPixelStoref:function
align 16
glPixelStoref:
invlpg [rax]
db "glPixelStoref", 0
ret

global glPixelStorei:function
align 16
glPixelStorei:
invlpg [rax]
db "glPixelStorei", 0
ret

global glPixelTransferf:function
align 16
glPixelTransferf:
invlpg [rax]
db "glPixelTransferf", 0
ret

global glPixelTransferi:function
align 16
glPixelTransferi:
invlpg [rax]
db "glPixelTransferi", 0
ret

global glPixelZoom:function
align 16
glPixelZoom:
invlpg [rax]
db "glPixelZoom", 0
ret

global glPointSize:function
align 16
glPointSize:
invlpg [rax]
db "glPointSize", 0
ret

global glPolygonMode:function
align 16
glPolygonMode:
invlpg [rax]
db "glPolygonMode", 0
ret

global glPolygonStipple:function
align 16
glPolygonStipple:
invlpg [rax]
db "glPolygonStipple", 0
ret

global glPopAttrib:function
align 16
glPopAttrib:
invlpg [rax]
db "glPopAttrib", 0
ret

global glPopMatrix:function
align 16
glPopMatrix:
invlpg [rax]
db "glPopMatrix", 0
ret

global glPopName:function
align 16
glPopName:
invlpg [rax]
db "glPopName", 0
ret

global glPushAttrib:function
align 16
glPushAttrib:
invlpg [rax]
db "glPushAttrib", 0
ret

global glPushMatrix:function
align 16
glPushMatrix:
invlpg [rax]
db "glPushMatrix", 0
ret

global glPushName:function
align 16
glPushName:
invlpg [rax]
db "glPushName", 0
ret

global glRasterPos2d:function
align 16
glRasterPos2d:
invlpg [rax]
db "glRasterPos2d", 0
ret

global glRasterPos2dv:function
align 16
glRasterPos2dv:
invlpg [rax]
db "glRasterPos2dv", 0
ret

global glRasterPos2f:function
align 16
glRasterPos2f:
invlpg [rax]
db "glRasterPos2f", 0
ret

global glRasterPos2fv:function
align 16
glRasterPos2fv:
invlpg [rax]
db "glRasterPos2fv", 0
ret

global glRasterPos2i:function
align 16
glRasterPos2i:
invlpg [rax]
db "glRasterPos2i", 0
ret

global glRasterPos2iv:function
align 16
glRasterPos2iv:
invlpg [rax]
db "glRasterPos2iv", 0
ret

global glRasterPos2s:function
align 16
glRasterPos2s:
invlpg [rax]
db "glRasterPos2s", 0
ret

global glRasterPos2sv:function
align 16
glRasterPos2sv:
invlpg [rax]
db "glRasterPos2sv", 0
ret

global glRasterPos3d:function
align 16
glRasterPos3d:
invlpg [rax]
db "glRasterPos3d", 0
ret

global glRasterPos3dv:function
align 16
glRasterPos3dv:
invlpg [rax]
db "glRasterPos3dv", 0
ret

global glRasterPos3f:function
align 16
glRasterPos3f:
invlpg [rax]
db "glRasterPos3f", 0
ret

global glRasterPos3fv:function
align 16
glRasterPos3fv:
invlpg [rax]
db "glRasterPos3fv", 0
ret

global glRasterPos3i:function
align 16
glRasterPos3i:
invlpg [rax]
db "glRasterPos3i", 0
ret

global glRasterPos3iv:function
align 16
glRasterPos3iv:
invlpg [rax]
db "glRasterPos3iv", 0
ret

global glRasterPos3s:function
align 16
glRasterPos3s:
invlpg [rax]
db "glRasterPos3s", 0
ret

global glRasterPos3sv:function
align 16
glRasterPos3sv:
invlpg [rax]
db "glRasterPos3sv", 0
ret

global glRasterPos4d:function
align 16
glRasterPos4d:
invlpg [rax]
db "glRasterPos4d", 0
ret

global glRasterPos4dv:function
align 16
glRasterPos4dv:
invlpg [rax]
db "glRasterPos4dv", 0
ret

global glRasterPos4f:function
align 16
glRasterPos4f:
invlpg [rax]
db "glRasterPos4f", 0
ret

global glRasterPos4fv:function
align 16
glRasterPos4fv:
invlpg [rax]
db "glRasterPos4fv", 0
ret

global glRasterPos4i:function
align 16
glRasterPos4i:
invlpg [rax]
db "glRasterPos4i", 0
ret

global glRasterPos4iv:function
align 16
glRasterPos4iv:
invlpg [rax]
db "glRasterPos4iv", 0
ret

global glRasterPos4s:function
align 16
glRasterPos4s:
invlpg [rax]
db "glRasterPos4s", 0
ret

global glRasterPos4sv:function
align 16
glRasterPos4sv:
invlpg [rax]
db "glRasterPos4sv", 0
ret

global glReadBuffer:function
align 16
glReadBuffer:
invlpg [rax]
db "glReadBuffer", 0
ret

global glReadPixels:function
align 16
glReadPixels:
invlpg [rax]
db "glReadPixels", 0
ret

global glRectd:function
align 16
glRectd:
invlpg [rax]
db "glRectd", 0
ret

global glRectdv:function
align 16
glRectdv:
invlpg [rax]
db "glRectdv", 0
ret

global glRectf:function
align 16
glRectf:
invlpg [rax]
db "glRectf", 0
ret

global glRectfv:function
align 16
glRectfv:
invlpg [rax]
db "glRectfv", 0
ret

global glRecti:function
align 16
glRecti:
invlpg [rax]
db "glRecti", 0
ret

global glRectiv:function
align 16
glRectiv:
invlpg [rax]
db "glRectiv", 0
ret

global glRects:function
align 16
glRects:
invlpg [rax]
db "glRects", 0
ret

global glRectsv:function
align 16
glRectsv:
invlpg [rax]
db "glRectsv", 0
ret

global glRenderMode:function
align 16
glRenderMode:
invlpg [rax]
db "glRenderMode", 0
ret

global glRotated:function
align 16
glRotated:
invlpg [rax]
db "glRotated", 0
ret

global glRotatef:function
align 16
glRotatef:
invlpg [rax]
db "glRotatef", 0
ret

global glScaled:function
align 16
glScaled:
invlpg [rax]
db "glScaled", 0
ret

global glScalef:function
align 16
glScalef:
invlpg [rax]
db "glScalef", 0
ret

global glScissor:function
align 16
glScissor:
invlpg [rax]
db "glScissor", 0
ret

global glSelectBuffer:function
align 16
glSelectBuffer:
invlpg [rax]
db "glSelectBuffer", 0
ret

global glShadeModel:function
align 16
glShadeModel:
invlpg [rax]
db "glShadeModel", 0
ret

global glStencilFunc:function
align 16
glStencilFunc:
invlpg [rax]
db "glStencilFunc", 0
ret

global glStencilMask:function
align 16
glStencilMask:
invlpg [rax]
db "glStencilMask", 0
ret

global glStencilOp:function
align 16
glStencilOp:
invlpg [rax]
db "glStencilOp", 0
ret

global glTexCoord1d:function
align 16
glTexCoord1d:
invlpg [rax]
db "glTexCoord1d", 0
ret

global glTexCoord1dv:function
align 16
glTexCoord1dv:
invlpg [rax]
db "glTexCoord1dv", 0
ret

global glTexCoord1f:function
align 16
glTexCoord1f:
invlpg [rax]
db "glTexCoord1f", 0
ret

global glTexCoord1fv:function
align 16
glTexCoord1fv:
invlpg [rax]
db "glTexCoord1fv", 0
ret

global glTexCoord1i:function
align 16
glTexCoord1i:
invlpg [rax]
db "glTexCoord1i", 0
ret

global glTexCoord1iv:function
align 16
glTexCoord1iv:
invlpg [rax]
db "glTexCoord1iv", 0
ret

global glTexCoord1s:function
align 16
glTexCoord1s:
invlpg [rax]
db "glTexCoord1s", 0
ret

global glTexCoord1sv:function
align 16
glTexCoord1sv:
invlpg [rax]
db "glTexCoord1sv", 0
ret

global glTexCoord2d:function
align 16
glTexCoord2d:
invlpg [rax]
db "glTexCoord2d", 0
ret

global glTexCoord2dv:function
align 16
glTexCoord2dv:
invlpg [rax]
db "glTexCoord2dv", 0
ret

global glTexCoord2f:function
align 16
glTexCoord2f:
invlpg [rax]
db "glTexCoord2f", 0
ret

global glTexCoord2fv:function
align 16
glTexCoord2fv:
invlpg [rax]
db "glTexCoord2fv", 0
ret

global glTexCoord2i:function
align 16
glTexCoord2i:
invlpg [rax]
db "glTexCoord2i", 0
ret

global glTexCoord2iv:function
align 16
glTexCoord2iv:
invlpg [rax]
db "glTexCoord2iv", 0
ret

global glTexCoord2s:function
align 16
glTexCoord2s:
invlpg [rax]
db "glTexCoord2s", 0
ret

global glTexCoord2sv:function
align 16
glTexCoord2sv:
invlpg [rax]
db "glTexCoord2sv", 0
ret

global glTexCoord3d:function
align 16
glTexCoord3d:
invlpg [rax]
db "glTexCoord3d", 0
ret

global glTexCoord3dv:function
align 16
glTexCoord3dv:
invlpg [rax]
db "glTexCoord3dv", 0
ret

global glTexCoord3f:function
align 16
glTexCoord3f:
invlpg [rax]
db "glTexCoord3f", 0
ret

global glTexCoord3fv:function
align 16
glTexCoord3fv:
invlpg [rax]
db "glTexCoord3fv", 0
ret

global glTexCoord3i:function
align 16
glTexCoord3i:
invlpg [rax]
db "glTexCoord3i", 0
ret

global glTexCoord3iv:function
align 16
glTexCoord3iv:
invlpg [rax]
db "glTexCoord3iv", 0
ret

global glTexCoord3s:function
align 16
glTexCoord3s:
invlpg [rax]
db "glTexCoord3s", 0
ret

global glTexCoord3sv:function
align 16
glTexCoord3sv:
invlpg [rax]
db "glTexCoord3sv", 0
ret

global glTexCoord4d:function
align 16
glTexCoord4d:
invlpg [rax]
db "glTexCoord4d", 0
ret

global glTexCoord4dv:function
align 16
glTexCoord4dv:
invlpg [rax]
db "glTexCoord4dv", 0
ret

global glTexCoord4f:function
align 16
glTexCoord4f:
invlpg [rax]
db "glTexCoord4f", 0
ret

global glTexCoord4fv:function
align 16
glTexCoord4fv:
invlpg [rax]
db "glTexCoord4fv", 0
ret

global glTexCoord4i:function
align 16
glTexCoord4i:
invlpg [rax]
db "glTexCoord4i", 0
ret

global glTexCoord4iv:function
align 16
glTexCoord4iv:
invlpg [rax]
db "glTexCoord4iv", 0
ret

global glTexCoord4s:function
align 16
glTexCoord4s:
invlpg [rax]
db "glTexCoord4s", 0
ret

global glTexCoord4sv:function
align 16
glTexCoord4sv:
invlpg [rax]
db "glTexCoord4sv", 0
ret

global glTexEnvf:function
align 16
glTexEnvf:
invlpg [rax]
db "glTexEnvf", 0
ret

global glTexEnvfv:function
align 16
glTexEnvfv:
invlpg [rax]
db "glTexEnvfv", 0
ret

global glTexEnvi:function
align 16
glTexEnvi:
invlpg [rax]
db "glTexEnvi", 0
ret

global glTexEnviv:function
align 16
glTexEnviv:
invlpg [rax]
db "glTexEnviv", 0
ret

global glTexGend:function
align 16
glTexGend:
invlpg [rax]
db "glTexGend", 0
ret

global glTexGendv:function
align 16
glTexGendv:
invlpg [rax]
db "glTexGendv", 0
ret

global glTexGenf:function
align 16
glTexGenf:
invlpg [rax]
db "glTexGenf", 0
ret

global glTexGenfv:function
align 16
glTexGenfv:
invlpg [rax]
db "glTexGenfv", 0
ret

global glTexGeni:function
align 16
glTexGeni:
invlpg [rax]
db "glTexGeni", 0
ret

global glTexGeniv:function
align 16
glTexGeniv:
invlpg [rax]
db "glTexGeniv", 0
ret

global glTexImage1D:function
align 16
glTexImage1D:
invlpg [rax]
db "glTexImage1D", 0
ret

global glTexImage2D:function
align 16
glTexImage2D:
invlpg [rax]
db "glTexImage2D", 0
ret

global glTexParameterf:function
align 16
glTexParameterf:
invlpg [rax]
db "glTexParameterf", 0
ret

global glTexParameterfv:function
align 16
glTexParameterfv:
invlpg [rax]
db "glTexParameterfv", 0
ret

global glTexParameteri:function
align 16
glTexParameteri:
invlpg [rax]
db "glTexParameteri", 0
ret

global glTexParameteriv:function
align 16
glTexParameteriv:
invlpg [rax]
db "glTexParameteriv", 0
ret

global glTranslated:function
align 16
glTranslated:
invlpg [rax]
db "glTranslated", 0
ret

global glTranslatef:function
align 16
glTranslatef:
invlpg [rax]
db "glTranslatef", 0
ret

global glVertex2d:function
align 16
glVertex2d:
invlpg [rax]
db "glVertex2d", 0
ret

global glVertex2dv:function
align 16
glVertex2dv:
invlpg [rax]
db "glVertex2dv", 0
ret

global glVertex2f:function
align 16
glVertex2f:
invlpg [rax]
db "glVertex2f", 0
ret

global glVertex2fv:function
align 16
glVertex2fv:
invlpg [rax]
db "glVertex2fv", 0
ret

global glVertex2i:function
align 16
glVertex2i:
invlpg [rax]
db "glVertex2i", 0
ret

global glVertex2iv:function
align 16
glVertex2iv:
invlpg [rax]
db "glVertex2iv", 0
ret

global glVertex2s:function
align 16
glVertex2s:
invlpg [rax]
db "glVertex2s", 0
ret

global glVertex2sv:function
align 16
glVertex2sv:
invlpg [rax]
db "glVertex2sv", 0
ret

global glVertex3d:function
align 16
glVertex3d:
invlpg [rax]
db "glVertex3d", 0
ret

global glVertex3dv:function
align 16
glVertex3dv:
invlpg [rax]
db "glVertex3dv", 0
ret

global glVertex3f:function
align 16
glVertex3f:
invlpg [rax]
db "glVertex3f", 0
ret

global glVertex3fv:function
align 16
glVertex3fv:
invlpg [rax]
db "glVertex3fv", 0
ret

global glVertex3i:function
align 16
glVertex3i:
invlpg [rax]
db "glVertex3i", 0
ret

global glVertex3iv:function
align 16
glVertex3iv:
invlpg [rax]
db "glVertex3iv", 0
ret

global glVertex3s:function
align 16
glVertex3s:
invlpg [rax]
db "glVertex3s", 0
ret

global glVertex3sv:function
align 16
glVertex3sv:
invlpg [rax]
db "glVertex3sv", 0
ret

global glVertex4d:function
align 16
glVertex4d:
invlpg [rax]
db "glVertex4d", 0
ret

global glVertex4dv:function
align 16
glVertex4dv:
invlpg [rax]
db "glVertex4dv", 0
ret

global glVertex4f:function
align 16
glVertex4f:
invlpg [rax]
db "glVertex4f", 0
ret

global glVertex4fv:function
align 16
glVertex4fv:
invlpg [rax]
db "glVertex4fv", 0
ret

global glVertex4i:function
align 16
glVertex4i:
invlpg [rax]
db "glVertex4i", 0
ret

global glVertex4iv:function
align 16
glVertex4iv:
invlpg [rax]
db "glVertex4iv", 0
ret

global glVertex4s:function
align 16
glVertex4s:
invlpg [rax]
db "glVertex4s", 0
ret

global glVertex4sv:function
align 16
glVertex4sv:
invlpg [rax]
db "glVertex4sv", 0
ret

global glViewport:function
align 16
glViewport:
invlpg [rax]
db "glViewport", 0
ret

global glAreTexturesResident:function
align 16
glAreTexturesResident:
invlpg [rax]
db "glAreTexturesResident", 0
ret

global glArrayElement:function
align 16
glArrayElement:
invlpg [rax]
db "glArrayElement", 0
ret

global glBindTexture:function
align 16
glBindTexture:
invlpg [rax]
db "glBindTexture", 0
ret

global glColorPointer:function
align 16
glColorPointer:
invlpg [rax]
db "glColorPointer", 0
ret

global glCopyTexImage1D:function
align 16
glCopyTexImage1D:
invlpg [rax]
db "glCopyTexImage1D", 0
ret

global glCopyTexImage2D:function
align 16
glCopyTexImage2D:
invlpg [rax]
db "glCopyTexImage2D", 0
ret

global glCopyTexSubImage1D:function
align 16
glCopyTexSubImage1D:
invlpg [rax]
db "glCopyTexSubImage1D", 0
ret

global glCopyTexSubImage2D:function
align 16
glCopyTexSubImage2D:
invlpg [rax]
db "glCopyTexSubImage2D", 0
ret

global glDeleteTextures:function
align 16
glDeleteTextures:
invlpg [rax]
db "glDeleteTextures", 0
ret

global glDisableClientState:function
align 16
glDisableClientState:
invlpg [rax]
db "glDisableClientState", 0
ret

global glDrawArrays:function
align 16
glDrawArrays:
invlpg [rax]
db "glDrawArrays", 0
ret

global glDrawElements:function
align 16
glDrawElements:
invlpg [rax]
db "glDrawElements", 0
ret

global glEdgeFlagPointer:function
align 16
glEdgeFlagPointer:
invlpg [rax]
db "glEdgeFlagPointer", 0
ret

global glEnableClientState:function
align 16
glEnableClientState:
invlpg [rax]
db "glEnableClientState", 0
ret

global glGenTextures:function
align 16
glGenTextures:
invlpg [rax]
db "glGenTextures", 0
ret

global glGetPointerv:function
align 16
glGetPointerv:
invlpg [rax]
db "glGetPointerv", 0
ret

global glIndexPointer:function
align 16
glIndexPointer:
invlpg [rax]
db "glIndexPointer", 0
ret

global glIndexub:function
align 16
glIndexub:
invlpg [rax]
db "glIndexub", 0
ret

global glIndexubv:function
align 16
glIndexubv:
invlpg [rax]
db "glIndexubv", 0
ret

global glInterleavedArrays:function
align 16
glInterleavedArrays:
invlpg [rax]
db "glInterleavedArrays", 0
ret

global glIsTexture:function
align 16
glIsTexture:
invlpg [rax]
db "glIsTexture", 0
ret

global glNormalPointer:function
align 16
glNormalPointer:
invlpg [rax]
db "glNormalPointer", 0
ret

global glPolygonOffset:function
align 16
glPolygonOffset:
invlpg [rax]
db "glPolygonOffset", 0
ret

global glPopClientAttrib:function
align 16
glPopClientAttrib:
invlpg [rax]
db "glPopClientAttrib", 0
ret

global glPrioritizeTextures:function
align 16
glPrioritizeTextures:
invlpg [rax]
db "glPrioritizeTextures", 0
ret

global glPushClientAttrib:function
align 16
glPushClientAttrib:
invlpg [rax]
db "glPushClientAttrib", 0
ret

global glTexCoordPointer:function
align 16
glTexCoordPointer:
invlpg [rax]
db "glTexCoordPointer", 0
ret

global glTexSubImage1D:function
align 16
glTexSubImage1D:
invlpg [rax]
db "glTexSubImage1D", 0
ret

global glTexSubImage2D:function
align 16
glTexSubImage2D:
invlpg [rax]
db "glTexSubImage2D", 0
ret

global glVertexPointer:function
align 16
glVertexPointer:
invlpg [rax]
db "glVertexPointer", 0
ret

global glBlendColor:function
align 16
glBlendColor:
invlpg [rax]
db "glBlendColor", 0
ret

global glBlendEquation:function
align 16
glBlendEquation:
invlpg [rax]
db "glBlendEquation", 0
ret

global glCopyTexSubImage3D:function
align 16
glCopyTexSubImage3D:
invlpg [rax]
db "glCopyTexSubImage3D", 0
ret

global glDrawRangeElements:function
align 16
glDrawRangeElements:
invlpg [rax]
db "glDrawRangeElements", 0
ret

global glTexImage3D:function
align 16
glTexImage3D:
invlpg [rax]
db "glTexImage3D", 0
ret

global glTexSubImage3D:function
align 16
glTexSubImage3D:
invlpg [rax]
db "glTexSubImage3D", 0
ret

global glActiveTexture:function
align 16
glActiveTexture:
invlpg [rax]
db "glActiveTexture", 0
ret

global glClientActiveTexture:function
align 16
glClientActiveTexture:
invlpg [rax]
db "glClientActiveTexture", 0
ret

global glCompressedTexImage1D:function
align 16
glCompressedTexImage1D:
invlpg [rax]
db "glCompressedTexImage1D", 0
ret

global glCompressedTexImage2D:function
align 16
glCompressedTexImage2D:
invlpg [rax]
db "glCompressedTexImage2D", 0
ret

global glCompressedTexImage3D:function
align 16
glCompressedTexImage3D:
invlpg [rax]
db "glCompressedTexImage3D", 0
ret

global glCompressedTexSubImage1D:function
align 16
glCompressedTexSubImage1D:
invlpg [rax]
db "glCompressedTexSubImage1D", 0
ret

global glCompressedTexSubImage2D:function
align 16
glCompressedTexSubImage2D:
invlpg [rax]
db "glCompressedTexSubImage2D", 0
ret

global glCompressedTexSubImage3D:function
align 16
glCompressedTexSubImage3D:
invlpg [rax]
db "glCompressedTexSubImage3D", 0
ret

global glGetCompressedTexImage:function
align 16
glGetCompressedTexImage:
invlpg [rax]
db "glGetCompressedTexImage", 0
ret

global glLoadTransposeMatrixd:function
align 16
glLoadTransposeMatrixd:
invlpg [rax]
db "glLoadTransposeMatrixd", 0
ret

global glLoadTransposeMatrixf:function
align 16
glLoadTransposeMatrixf:
invlpg [rax]
db "glLoadTransposeMatrixf", 0
ret

global glMultTransposeMatrixd:function
align 16
glMultTransposeMatrixd:
invlpg [rax]
db "glMultTransposeMatrixd", 0
ret

global glMultTransposeMatrixf:function
align 16
glMultTransposeMatrixf:
invlpg [rax]
db "glMultTransposeMatrixf", 0
ret

global glMultiTexCoord1d:function
align 16
glMultiTexCoord1d:
invlpg [rax]
db "glMultiTexCoord1d", 0
ret

global glMultiTexCoord1dv:function
align 16
glMultiTexCoord1dv:
invlpg [rax]
db "glMultiTexCoord1dv", 0
ret

global glMultiTexCoord1f:function
align 16
glMultiTexCoord1f:
invlpg [rax]
db "glMultiTexCoord1f", 0
ret

global glMultiTexCoord1fv:function
align 16
glMultiTexCoord1fv:
invlpg [rax]
db "glMultiTexCoord1fv", 0
ret

global glMultiTexCoord1i:function
align 16
glMultiTexCoord1i:
invlpg [rax]
db "glMultiTexCoord1i", 0
ret

global glMultiTexCoord1iv:function
align 16
glMultiTexCoord1iv:
invlpg [rax]
db "glMultiTexCoord1iv", 0
ret

global glMultiTexCoord1s:function
align 16
glMultiTexCoord1s:
invlpg [rax]
db "glMultiTexCoord1s", 0
ret

global glMultiTexCoord1sv:function
align 16
glMultiTexCoord1sv:
invlpg [rax]
db "glMultiTexCoord1sv", 0
ret

global glMultiTexCoord2d:function
align 16
glMultiTexCoord2d:
invlpg [rax]
db "glMultiTexCoord2d", 0
ret

global glMultiTexCoord2dv:function
align 16
glMultiTexCoord2dv:
invlpg [rax]
db "glMultiTexCoord2dv", 0
ret

global glMultiTexCoord2f:function
align 16
glMultiTexCoord2f:
invlpg [rax]
db "glMultiTexCoord2f", 0
ret

global glMultiTexCoord2fv:function
align 16
glMultiTexCoord2fv:
invlpg [rax]
db "glMultiTexCoord2fv", 0
ret

global glMultiTexCoord2i:function
align 16
glMultiTexCoord2i:
invlpg [rax]
db "glMultiTexCoord2i", 0
ret

global glMultiTexCoord2iv:function
align 16
glMultiTexCoord2iv:
invlpg [rax]
db "glMultiTexCoord2iv", 0
ret

global glMultiTexCoord2s:function
align 16
glMultiTexCoord2s:
invlpg [rax]
db "glMultiTexCoord2s", 0
ret

global glMultiTexCoord2sv:function
align 16
glMultiTexCoord2sv:
invlpg [rax]
db "glMultiTexCoord2sv", 0
ret

global glMultiTexCoord3d:function
align 16
glMultiTexCoord3d:
invlpg [rax]
db "glMultiTexCoord3d", 0
ret

global glMultiTexCoord3dv:function
align 16
glMultiTexCoord3dv:
invlpg [rax]
db "glMultiTexCoord3dv", 0
ret

global glMultiTexCoord3f:function
align 16
glMultiTexCoord3f:
invlpg [rax]
db "glMultiTexCoord3f", 0
ret

global glMultiTexCoord3fv:function
align 16
glMultiTexCoord3fv:
invlpg [rax]
db "glMultiTexCoord3fv", 0
ret

global glMultiTexCoord3i:function
align 16
glMultiTexCoord3i:
invlpg [rax]
db "glMultiTexCoord3i", 0
ret

global glMultiTexCoord3iv:function
align 16
glMultiTexCoord3iv:
invlpg [rax]
db "glMultiTexCoord3iv", 0
ret

global glMultiTexCoord3s:function
align 16
glMultiTexCoord3s:
invlpg [rax]
db "glMultiTexCoord3s", 0
ret

global glMultiTexCoord3sv:function
align 16
glMultiTexCoord3sv:
invlpg [rax]
db "glMultiTexCoord3sv", 0
ret

global glMultiTexCoord4d:function
align 16
glMultiTexCoord4d:
invlpg [rax]
db "glMultiTexCoord4d", 0
ret

global glMultiTexCoord4dv:function
align 16
glMultiTexCoord4dv:
invlpg [rax]
db "glMultiTexCoord4dv", 0
ret

global glMultiTexCoord4f:function
align 16
glMultiTexCoord4f:
invlpg [rax]
db "glMultiTexCoord4f", 0
ret

global glMultiTexCoord4fv:function
align 16
glMultiTexCoord4fv:
invlpg [rax]
db "glMultiTexCoord4fv", 0
ret

global glMultiTexCoord4i:function
align 16
glMultiTexCoord4i:
invlpg [rax]
db "glMultiTexCoord4i", 0
ret

global glMultiTexCoord4iv:function
align 16
glMultiTexCoord4iv:
invlpg [rax]
db "glMultiTexCoord4iv", 0
ret

global glMultiTexCoord4s:function
align 16
glMultiTexCoord4s:
invlpg [rax]
db "glMultiTexCoord4s", 0
ret

global glMultiTexCoord4sv:function
align 16
glMultiTexCoord4sv:
invlpg [rax]
db "glMultiTexCoord4sv", 0
ret

global glSampleCoverage:function
align 16
glSampleCoverage:
invlpg [rax]
db "glSampleCoverage", 0
ret

global glBlendFuncSeparate:function
align 16
glBlendFuncSeparate:
invlpg [rax]
db "glBlendFuncSeparate", 0
ret

global glFogCoordPointer:function
align 16
glFogCoordPointer:
invlpg [rax]
db "glFogCoordPointer", 0
ret

global glFogCoordd:function
align 16
glFogCoordd:
invlpg [rax]
db "glFogCoordd", 0
ret

global glFogCoorddv:function
align 16
glFogCoorddv:
invlpg [rax]
db "glFogCoorddv", 0
ret

global glFogCoordf:function
align 16
glFogCoordf:
invlpg [rax]
db "glFogCoordf", 0
ret

global glFogCoordfv:function
align 16
glFogCoordfv:
invlpg [rax]
db "glFogCoordfv", 0
ret

global glMultiDrawArrays:function
align 16
glMultiDrawArrays:
invlpg [rax]
db "glMultiDrawArrays", 0
ret

global glMultiDrawElements:function
align 16
glMultiDrawElements:
invlpg [rax]
db "glMultiDrawElements", 0
ret

global glPointParameterf:function
align 16
glPointParameterf:
invlpg [rax]
db "glPointParameterf", 0
ret

global glPointParameterfv:function
align 16
glPointParameterfv:
invlpg [rax]
db "glPointParameterfv", 0
ret

global glPointParameteri:function
align 16
glPointParameteri:
invlpg [rax]
db "glPointParameteri", 0
ret

global glPointParameteriv:function
align 16
glPointParameteriv:
invlpg [rax]
db "glPointParameteriv", 0
ret

global glSecondaryColor3b:function
align 16
glSecondaryColor3b:
invlpg [rax]
db "glSecondaryColor3b", 0
ret

global glSecondaryColor3bv:function
align 16
glSecondaryColor3bv:
invlpg [rax]
db "glSecondaryColor3bv", 0
ret

global glSecondaryColor3d:function
align 16
glSecondaryColor3d:
invlpg [rax]
db "glSecondaryColor3d", 0
ret

global glSecondaryColor3dv:function
align 16
glSecondaryColor3dv:
invlpg [rax]
db "glSecondaryColor3dv", 0
ret

global glSecondaryColor3f:function
align 16
glSecondaryColor3f:
invlpg [rax]
db "glSecondaryColor3f", 0
ret

global glSecondaryColor3fv:function
align 16
glSecondaryColor3fv:
invlpg [rax]
db "glSecondaryColor3fv", 0
ret

global glSecondaryColor3i:function
align 16
glSecondaryColor3i:
invlpg [rax]
db "glSecondaryColor3i", 0
ret

global glSecondaryColor3iv:function
align 16
glSecondaryColor3iv:
invlpg [rax]
db "glSecondaryColor3iv", 0
ret

global glSecondaryColor3s:function
align 16
glSecondaryColor3s:
invlpg [rax]
db "glSecondaryColor3s", 0
ret

global glSecondaryColor3sv:function
align 16
glSecondaryColor3sv:
invlpg [rax]
db "glSecondaryColor3sv", 0
ret

global glSecondaryColor3ub:function
align 16
glSecondaryColor3ub:
invlpg [rax]
db "glSecondaryColor3ub", 0
ret

global glSecondaryColor3ubv:function
align 16
glSecondaryColor3ubv:
invlpg [rax]
db "glSecondaryColor3ubv", 0
ret

global glSecondaryColor3ui:function
align 16
glSecondaryColor3ui:
invlpg [rax]
db "glSecondaryColor3ui", 0
ret

global glSecondaryColor3uiv:function
align 16
glSecondaryColor3uiv:
invlpg [rax]
db "glSecondaryColor3uiv", 0
ret

global glSecondaryColor3us:function
align 16
glSecondaryColor3us:
invlpg [rax]
db "glSecondaryColor3us", 0
ret

global glSecondaryColor3usv:function
align 16
glSecondaryColor3usv:
invlpg [rax]
db "glSecondaryColor3usv", 0
ret

global glSecondaryColorPointer:function
align 16
glSecondaryColorPointer:
invlpg [rax]
db "glSecondaryColorPointer", 0
ret

global glWindowPos2d:function
align 16
glWindowPos2d:
invlpg [rax]
db "glWindowPos2d", 0
ret

global glWindowPos2dv:function
align 16
glWindowPos2dv:
invlpg [rax]
db "glWindowPos2dv", 0
ret

global glWindowPos2f:function
align 16
glWindowPos2f:
invlpg [rax]
db "glWindowPos2f", 0
ret

global glWindowPos2fv:function
align 16
glWindowPos2fv:
invlpg [rax]
db "glWindowPos2fv", 0
ret

global glWindowPos2i:function
align 16
glWindowPos2i:
invlpg [rax]
db "glWindowPos2i", 0
ret

global glWindowPos2iv:function
align 16
glWindowPos2iv:
invlpg [rax]
db "glWindowPos2iv", 0
ret

global glWindowPos2s:function
align 16
glWindowPos2s:
invlpg [rax]
db "glWindowPos2s", 0
ret

global glWindowPos2sv:function
align 16
glWindowPos2sv:
invlpg [rax]
db "glWindowPos2sv", 0
ret

global glWindowPos3d:function
align 16
glWindowPos3d:
invlpg [rax]
db "glWindowPos3d", 0
ret

global glWindowPos3dv:function
align 16
glWindowPos3dv:
invlpg [rax]
db "glWindowPos3dv", 0
ret

global glWindowPos3f:function
align 16
glWindowPos3f:
invlpg [rax]
db "glWindowPos3f", 0
ret

global glWindowPos3fv:function
align 16
glWindowPos3fv:
invlpg [rax]
db "glWindowPos3fv", 0
ret

global glWindowPos3i:function
align 16
glWindowPos3i:
invlpg [rax]
db "glWindowPos3i", 0
ret

global glWindowPos3iv:function
align 16
glWindowPos3iv:
invlpg [rax]
db "glWindowPos3iv", 0
ret

global glWindowPos3s:function
align 16
glWindowPos3s:
invlpg [rax]
db "glWindowPos3s", 0
ret

global glWindowPos3sv:function
align 16
glWindowPos3sv:
invlpg [rax]
db "glWindowPos3sv", 0
ret

global glBeginQuery:function
align 16
glBeginQuery:
invlpg [rax]
db "glBeginQuery", 0
ret

global glBindBuffer:function
align 16
glBindBuffer:
invlpg [rax]
db "glBindBuffer", 0
ret

global glBufferData:function
align 16
glBufferData:
invlpg [rax]
db "glBufferData", 0
ret

global glBufferSubData:function
align 16
glBufferSubData:
invlpg [rax]
db "glBufferSubData", 0
ret

global glDeleteBuffers:function
align 16
glDeleteBuffers:
invlpg [rax]
db "glDeleteBuffers", 0
ret

global glDeleteQueries:function
align 16
glDeleteQueries:
invlpg [rax]
db "glDeleteQueries", 0
ret

global glEndQuery:function
align 16
glEndQuery:
invlpg [rax]
db "glEndQuery", 0
ret

global glGenBuffers:function
align 16
glGenBuffers:
invlpg [rax]
db "glGenBuffers", 0
ret

global glGenQueries:function
align 16
glGenQueries:
invlpg [rax]
db "glGenQueries", 0
ret

global glGetBufferParameteriv:function
align 16
glGetBufferParameteriv:
invlpg [rax]
db "glGetBufferParameteriv", 0
ret

global glGetBufferPointerv:function
align 16
glGetBufferPointerv:
invlpg [rax]
db "glGetBufferPointerv", 0
ret

global glGetBufferSubData:function
align 16
glGetBufferSubData:
invlpg [rax]
db "glGetBufferSubData", 0
ret

global glGetQueryObjectiv:function
align 16
glGetQueryObjectiv:
invlpg [rax]
db "glGetQueryObjectiv", 0
ret

global glGetQueryObjectuiv:function
align 16
glGetQueryObjectuiv:
invlpg [rax]
db "glGetQueryObjectuiv", 0
ret

global glGetQueryiv:function
align 16
glGetQueryiv:
invlpg [rax]
db "glGetQueryiv", 0
ret

global glIsBuffer:function
align 16
glIsBuffer:
invlpg [rax]
db "glIsBuffer", 0
ret

global glIsQuery:function
align 16
glIsQuery:
invlpg [rax]
db "glIsQuery", 0
ret

global glMapBuffer:function
align 16
glMapBuffer:
invlpg [rax]
db "glMapBuffer", 0
ret

global glUnmapBuffer:function
align 16
glUnmapBuffer:
invlpg [rax]
db "glUnmapBuffer", 0
ret

global glAttachShader:function
align 16
glAttachShader:
invlpg [rax]
db "glAttachShader", 0
ret

global glBindAttribLocation:function
align 16
glBindAttribLocation:
invlpg [rax]
db "glBindAttribLocation", 0
ret

global glBlendEquationSeparate:function
align 16
glBlendEquationSeparate:
invlpg [rax]
db "glBlendEquationSeparate", 0
ret

global glCompileShader:function
align 16
glCompileShader:
invlpg [rax]
db "glCompileShader", 0
ret

global glCreateProgram:function
align 16
glCreateProgram:
invlpg [rax]
db "glCreateProgram", 0
ret

global glCreateShader:function
align 16
glCreateShader:
invlpg [rax]
db "glCreateShader", 0
ret

global glDeleteProgram:function
align 16
glDeleteProgram:
invlpg [rax]
db "glDeleteProgram", 0
ret

global glDeleteShader:function
align 16
glDeleteShader:
invlpg [rax]
db "glDeleteShader", 0
ret

global glDetachShader:function
align 16
glDetachShader:
invlpg [rax]
db "glDetachShader", 0
ret

global glDisableVertexAttribArray:function
align 16
glDisableVertexAttribArray:
invlpg [rax]
db "glDisableVertexAttribArray", 0
ret

global glDrawBuffers:function
align 16
glDrawBuffers:
invlpg [rax]
db "glDrawBuffers", 0
ret

global glEnableVertexAttribArray:function
align 16
glEnableVertexAttribArray:
invlpg [rax]
db "glEnableVertexAttribArray", 0
ret

global glGetActiveAttrib:function
align 16
glGetActiveAttrib:
invlpg [rax]
db "glGetActiveAttrib", 0
ret

global glGetActiveUniform:function
align 16
glGetActiveUniform:
invlpg [rax]
db "glGetActiveUniform", 0
ret

global glGetAttachedShaders:function
align 16
glGetAttachedShaders:
invlpg [rax]
db "glGetAttachedShaders", 0
ret

global glGetAttribLocation:function
align 16
glGetAttribLocation:
invlpg [rax]
db "glGetAttribLocation", 0
ret

global glGetProgramInfoLog:function
align 16
glGetProgramInfoLog:
invlpg [rax]
db "glGetProgramInfoLog", 0
ret

global glGetProgramiv:function
align 16
glGetProgramiv:
invlpg [rax]
db "glGetProgramiv", 0
ret

global glGetShaderInfoLog:function
align 16
glGetShaderInfoLog:
invlpg [rax]
db "glGetShaderInfoLog", 0
ret

global glGetShaderSource:function
align 16
glGetShaderSource:
invlpg [rax]
db "glGetShaderSource", 0
ret

global glGetShaderiv:function
align 16
glGetShaderiv:
invlpg [rax]
db "glGetShaderiv", 0
ret

global glGetUniformLocation:function
align 16
glGetUniformLocation:
invlpg [rax]
db "glGetUniformLocation", 0
ret

global glGetUniformfv:function
align 16
glGetUniformfv:
invlpg [rax]
db "glGetUniformfv", 0
ret

global glGetUniformiv:function
align 16
glGetUniformiv:
invlpg [rax]
db "glGetUniformiv", 0
ret

global glGetVertexAttribPointerv:function
align 16
glGetVertexAttribPointerv:
invlpg [rax]
db "glGetVertexAttribPointerv", 0
ret

global glGetVertexAttribdv:function
align 16
glGetVertexAttribdv:
invlpg [rax]
db "glGetVertexAttribdv", 0
ret

global glGetVertexAttribfv:function
align 16
glGetVertexAttribfv:
invlpg [rax]
db "glGetVertexAttribfv", 0
ret

global glGetVertexAttribiv:function
align 16
glGetVertexAttribiv:
invlpg [rax]
db "glGetVertexAttribiv", 0
ret

global glIsProgram:function
align 16
glIsProgram:
invlpg [rax]
db "glIsProgram", 0
ret

global glIsShader:function
align 16
glIsShader:
invlpg [rax]
db "glIsShader", 0
ret

global glLinkProgram:function
align 16
glLinkProgram:
invlpg [rax]
db "glLinkProgram", 0
ret

global glShaderSource:function
align 16
glShaderSource:
invlpg [rax]
db "glShaderSource", 0
ret

global glStencilFuncSeparate:function
align 16
glStencilFuncSeparate:
invlpg [rax]
db "glStencilFuncSeparate", 0
ret

global glStencilMaskSeparate:function
align 16
glStencilMaskSeparate:
invlpg [rax]
db "glStencilMaskSeparate", 0
ret

global glStencilOpSeparate:function
align 16
glStencilOpSeparate:
invlpg [rax]
db "glStencilOpSeparate", 0
ret

global glUniform1f:function
align 16
glUniform1f:
invlpg [rax]
db "glUniform1f", 0
ret

global glUniform1fv:function
align 16
glUniform1fv:
invlpg [rax]
db "glUniform1fv", 0
ret

global glUniform1i:function
align 16
glUniform1i:
invlpg [rax]
db "glUniform1i", 0
ret

global glUniform1iv:function
align 16
glUniform1iv:
invlpg [rax]
db "glUniform1iv", 0
ret

global glUniform2f:function
align 16
glUniform2f:
invlpg [rax]
db "glUniform2f", 0
ret

global glUniform2fv:function
align 16
glUniform2fv:
invlpg [rax]
db "glUniform2fv", 0
ret

global glUniform2i:function
align 16
glUniform2i:
invlpg [rax]
db "glUniform2i", 0
ret

global glUniform2iv:function
align 16
glUniform2iv:
invlpg [rax]
db "glUniform2iv", 0
ret

global glUniform3f:function
align 16
glUniform3f:
invlpg [rax]
db "glUniform3f", 0
ret

global glUniform3fv:function
align 16
glUniform3fv:
invlpg [rax]
db "glUniform3fv", 0
ret

global glUniform3i:function
align 16
glUniform3i:
invlpg [rax]
db "glUniform3i", 0
ret

global glUniform3iv:function
align 16
glUniform3iv:
invlpg [rax]
db "glUniform3iv", 0
ret

global glUniform4f:function
align 16
glUniform4f:
invlpg [rax]
db "glUniform4f", 0
ret

global glUniform4fv:function
align 16
glUniform4fv:
invlpg [rax]
db "glUniform4fv", 0
ret

global glUniform4i:function
align 16
glUniform4i:
invlpg [rax]
db "glUniform4i", 0
ret

global glUniform4iv:function
align 16
glUniform4iv:
invlpg [rax]
db "glUniform4iv", 0
ret

global glUniformMatrix2fv:function
align 16
glUniformMatrix2fv:
invlpg [rax]
db "glUniformMatrix2fv", 0
ret

global glUniformMatrix3fv:function
align 16
glUniformMatrix3fv:
invlpg [rax]
db "glUniformMatrix3fv", 0
ret

global glUniformMatrix4fv:function
align 16
glUniformMatrix4fv:
invlpg [rax]
db "glUniformMatrix4fv", 0
ret

global glUseProgram:function
align 16
glUseProgram:
invlpg [rax]
db "glUseProgram", 0
ret

global glValidateProgram:function
align 16
glValidateProgram:
invlpg [rax]
db "glValidateProgram", 0
ret

global glVertexAttrib1d:function
align 16
glVertexAttrib1d:
invlpg [rax]
db "glVertexAttrib1d", 0
ret

global glVertexAttrib1dv:function
align 16
glVertexAttrib1dv:
invlpg [rax]
db "glVertexAttrib1dv", 0
ret

global glVertexAttrib1f:function
align 16
glVertexAttrib1f:
invlpg [rax]
db "glVertexAttrib1f", 0
ret

global glVertexAttrib1fv:function
align 16
glVertexAttrib1fv:
invlpg [rax]
db "glVertexAttrib1fv", 0
ret

global glVertexAttrib1s:function
align 16
glVertexAttrib1s:
invlpg [rax]
db "glVertexAttrib1s", 0
ret

global glVertexAttrib1sv:function
align 16
glVertexAttrib1sv:
invlpg [rax]
db "glVertexAttrib1sv", 0
ret

global glVertexAttrib2d:function
align 16
glVertexAttrib2d:
invlpg [rax]
db "glVertexAttrib2d", 0
ret

global glVertexAttrib2dv:function
align 16
glVertexAttrib2dv:
invlpg [rax]
db "glVertexAttrib2dv", 0
ret

global glVertexAttrib2f:function
align 16
glVertexAttrib2f:
invlpg [rax]
db "glVertexAttrib2f", 0
ret

global glVertexAttrib2fv:function
align 16
glVertexAttrib2fv:
invlpg [rax]
db "glVertexAttrib2fv", 0
ret

global glVertexAttrib2s:function
align 16
glVertexAttrib2s:
invlpg [rax]
db "glVertexAttrib2s", 0
ret

global glVertexAttrib2sv:function
align 16
glVertexAttrib2sv:
invlpg [rax]
db "glVertexAttrib2sv", 0
ret

global glVertexAttrib3d:function
align 16
glVertexAttrib3d:
invlpg [rax]
db "glVertexAttrib3d", 0
ret

global glVertexAttrib3dv:function
align 16
glVertexAttrib3dv:
invlpg [rax]
db "glVertexAttrib3dv", 0
ret

global glVertexAttrib3f:function
align 16
glVertexAttrib3f:
invlpg [rax]
db "glVertexAttrib3f", 0
ret

global glVertexAttrib3fv:function
align 16
glVertexAttrib3fv:
invlpg [rax]
db "glVertexAttrib3fv", 0
ret

global glVertexAttrib3s:function
align 16
glVertexAttrib3s:
invlpg [rax]
db "glVertexAttrib3s", 0
ret

global glVertexAttrib3sv:function
align 16
glVertexAttrib3sv:
invlpg [rax]
db "glVertexAttrib3sv", 0
ret

global glVertexAttrib4Nbv:function
align 16
glVertexAttrib4Nbv:
invlpg [rax]
db "glVertexAttrib4Nbv", 0
ret

global glVertexAttrib4Niv:function
align 16
glVertexAttrib4Niv:
invlpg [rax]
db "glVertexAttrib4Niv", 0
ret

global glVertexAttrib4Nsv:function
align 16
glVertexAttrib4Nsv:
invlpg [rax]
db "glVertexAttrib4Nsv", 0
ret

global glVertexAttrib4Nub:function
align 16
glVertexAttrib4Nub:
invlpg [rax]
db "glVertexAttrib4Nub", 0
ret

global glVertexAttrib4Nubv:function
align 16
glVertexAttrib4Nubv:
invlpg [rax]
db "glVertexAttrib4Nubv", 0
ret

global glVertexAttrib4Nuiv:function
align 16
glVertexAttrib4Nuiv:
invlpg [rax]
db "glVertexAttrib4Nuiv", 0
ret

global glVertexAttrib4Nusv:function
align 16
glVertexAttrib4Nusv:
invlpg [rax]
db "glVertexAttrib4Nusv", 0
ret

global glVertexAttrib4bv:function
align 16
glVertexAttrib4bv:
invlpg [rax]
db "glVertexAttrib4bv", 0
ret

global glVertexAttrib4d:function
align 16
glVertexAttrib4d:
invlpg [rax]
db "glVertexAttrib4d", 0
ret

global glVertexAttrib4dv:function
align 16
glVertexAttrib4dv:
invlpg [rax]
db "glVertexAttrib4dv", 0
ret

global glVertexAttrib4f:function
align 16
glVertexAttrib4f:
invlpg [rax]
db "glVertexAttrib4f", 0
ret

global glVertexAttrib4fv:function
align 16
glVertexAttrib4fv:
invlpg [rax]
db "glVertexAttrib4fv", 0
ret

global glVertexAttrib4iv:function
align 16
glVertexAttrib4iv:
invlpg [rax]
db "glVertexAttrib4iv", 0
ret

global glVertexAttrib4s:function
align 16
glVertexAttrib4s:
invlpg [rax]
db "glVertexAttrib4s", 0
ret

global glVertexAttrib4sv:function
align 16
glVertexAttrib4sv:
invlpg [rax]
db "glVertexAttrib4sv", 0
ret

global glVertexAttrib4ubv:function
align 16
glVertexAttrib4ubv:
invlpg [rax]
db "glVertexAttrib4ubv", 0
ret

global glVertexAttrib4uiv:function
align 16
glVertexAttrib4uiv:
invlpg [rax]
db "glVertexAttrib4uiv", 0
ret

global glVertexAttrib4usv:function
align 16
glVertexAttrib4usv:
invlpg [rax]
db "glVertexAttrib4usv", 0
ret

global glVertexAttribPointer:function
align 16
glVertexAttribPointer:
invlpg [rax]
db "glVertexAttribPointer", 0
ret

global glUniformMatrix2x3fv:function
align 16
glUniformMatrix2x3fv:
invlpg [rax]
db "glUniformMatrix2x3fv", 0
ret

global glUniformMatrix2x4fv:function
align 16
glUniformMatrix2x4fv:
invlpg [rax]
db "glUniformMatrix2x4fv", 0
ret

global glUniformMatrix3x2fv:function
align 16
glUniformMatrix3x2fv:
invlpg [rax]
db "glUniformMatrix3x2fv", 0
ret

global glUniformMatrix3x4fv:function
align 16
glUniformMatrix3x4fv:
invlpg [rax]
db "glUniformMatrix3x4fv", 0
ret

global glUniformMatrix4x2fv:function
align 16
glUniformMatrix4x2fv:
invlpg [rax]
db "glUniformMatrix4x2fv", 0
ret

global glUniformMatrix4x3fv:function
align 16
glUniformMatrix4x3fv:
invlpg [rax]
db "glUniformMatrix4x3fv", 0
ret

global glBeginConditionalRender:function
align 16
glBeginConditionalRender:
invlpg [rax]
db "glBeginConditionalRender", 0
ret

global glBeginTransformFeedback:function
align 16
glBeginTransformFeedback:
invlpg [rax]
db "glBeginTransformFeedback", 0
ret

global glBindBufferBase:function
align 16
glBindBufferBase:
invlpg [rax]
db "glBindBufferBase", 0
ret

global glBindBufferRange:function
align 16
glBindBufferRange:
invlpg [rax]
db "glBindBufferRange", 0
ret

global glBindFragDataLocation:function
align 16
glBindFragDataLocation:
invlpg [rax]
db "glBindFragDataLocation", 0
ret

global glClampColor:function
align 16
glClampColor:
invlpg [rax]
db "glClampColor", 0
ret

global glClearBufferfi:function
align 16
glClearBufferfi:
invlpg [rax]
db "glClearBufferfi", 0
ret

global glClearBufferfv:function
align 16
glClearBufferfv:
invlpg [rax]
db "glClearBufferfv", 0
ret

global glClearBufferiv:function
align 16
glClearBufferiv:
invlpg [rax]
db "glClearBufferiv", 0
ret

global glClearBufferuiv:function
align 16
glClearBufferuiv:
invlpg [rax]
db "glClearBufferuiv", 0
ret

global glColorMaski:function
align 16
glColorMaski:
invlpg [rax]
db "glColorMaski", 0
ret

global glDisablei:function
align 16
glDisablei:
invlpg [rax]
db "glDisablei", 0
ret

global glEnablei:function
align 16
glEnablei:
invlpg [rax]
db "glEnablei", 0
ret

global glEndConditionalRender:function
align 16
glEndConditionalRender:
invlpg [rax]
db "glEndConditionalRender", 0
ret

global glEndTransformFeedback:function
align 16
glEndTransformFeedback:
invlpg [rax]
db "glEndTransformFeedback", 0
ret

global glGetBooleani_v:function
align 16
glGetBooleani_v:
invlpg [rax]
db "glGetBooleani_v", 0
ret

global glGetFragDataLocation:function
align 16
glGetFragDataLocation:
invlpg [rax]
db "glGetFragDataLocation", 0
ret

global glGetIntegeri_v:function
align 16
glGetIntegeri_v:
invlpg [rax]
db "glGetIntegeri_v", 0
ret

global glGetStringi:function
align 16
glGetStringi:
invlpg [rax]
db "glGetStringi", 0
ret

global glGetTexParameterIiv:function
align 16
glGetTexParameterIiv:
invlpg [rax]
db "glGetTexParameterIiv", 0
ret

global glGetTexParameterIuiv:function
align 16
glGetTexParameterIuiv:
invlpg [rax]
db "glGetTexParameterIuiv", 0
ret

global glGetTransformFeedbackVarying:function
align 16
glGetTransformFeedbackVarying:
invlpg [rax]
db "glGetTransformFeedbackVarying", 0
ret

global glGetUniformuiv:function
align 16
glGetUniformuiv:
invlpg [rax]
db "glGetUniformuiv", 0
ret

global glGetVertexAttribIiv:function
align 16
glGetVertexAttribIiv:
invlpg [rax]
db "glGetVertexAttribIiv", 0
ret

global glGetVertexAttribIuiv:function
align 16
glGetVertexAttribIuiv:
invlpg [rax]
db "glGetVertexAttribIuiv", 0
ret

global glIsEnabledi:function
align 16
glIsEnabledi:
invlpg [rax]
db "glIsEnabledi", 0
ret

global glTexParameterIiv:function
align 16
glTexParameterIiv:
invlpg [rax]
db "glTexParameterIiv", 0
ret

global glTexParameterIuiv:function
align 16
glTexParameterIuiv:
invlpg [rax]
db "glTexParameterIuiv", 0
ret

global glTransformFeedbackVaryings:function
align 16
glTransformFeedbackVaryings:
invlpg [rax]
db "glTransformFeedbackVaryings", 0
ret

global glUniform1ui:function
align 16
glUniform1ui:
invlpg [rax]
db "glUniform1ui", 0
ret

global glUniform1uiv:function
align 16
glUniform1uiv:
invlpg [rax]
db "glUniform1uiv", 0
ret

global glUniform2ui:function
align 16
glUniform2ui:
invlpg [rax]
db "glUniform2ui", 0
ret

global glUniform2uiv:function
align 16
glUniform2uiv:
invlpg [rax]
db "glUniform2uiv", 0
ret

global glUniform3ui:function
align 16
glUniform3ui:
invlpg [rax]
db "glUniform3ui", 0
ret

global glUniform3uiv:function
align 16
glUniform3uiv:
invlpg [rax]
db "glUniform3uiv", 0
ret

global glUniform4ui:function
align 16
glUniform4ui:
invlpg [rax]
db "glUniform4ui", 0
ret

global glUniform4uiv:function
align 16
glUniform4uiv:
invlpg [rax]
db "glUniform4uiv", 0
ret

global glVertexAttribI1i:function
align 16
glVertexAttribI1i:
invlpg [rax]
db "glVertexAttribI1i", 0
ret

global glVertexAttribI1iv:function
align 16
glVertexAttribI1iv:
invlpg [rax]
db "glVertexAttribI1iv", 0
ret

global glVertexAttribI1ui:function
align 16
glVertexAttribI1ui:
invlpg [rax]
db "glVertexAttribI1ui", 0
ret

global glVertexAttribI1uiv:function
align 16
glVertexAttribI1uiv:
invlpg [rax]
db "glVertexAttribI1uiv", 0
ret

global glVertexAttribI2i:function
align 16
glVertexAttribI2i:
invlpg [rax]
db "glVertexAttribI2i", 0
ret

global glVertexAttribI2iv:function
align 16
glVertexAttribI2iv:
invlpg [rax]
db "glVertexAttribI2iv", 0
ret

global glVertexAttribI2ui:function
align 16
glVertexAttribI2ui:
invlpg [rax]
db "glVertexAttribI2ui", 0
ret

global glVertexAttribI2uiv:function
align 16
glVertexAttribI2uiv:
invlpg [rax]
db "glVertexAttribI2uiv", 0
ret

global glVertexAttribI3i:function
align 16
glVertexAttribI3i:
invlpg [rax]
db "glVertexAttribI3i", 0
ret

global glVertexAttribI3iv:function
align 16
glVertexAttribI3iv:
invlpg [rax]
db "glVertexAttribI3iv", 0
ret

global glVertexAttribI3ui:function
align 16
glVertexAttribI3ui:
invlpg [rax]
db "glVertexAttribI3ui", 0
ret

global glVertexAttribI3uiv:function
align 16
glVertexAttribI3uiv:
invlpg [rax]
db "glVertexAttribI3uiv", 0
ret

global glVertexAttribI4bv:function
align 16
glVertexAttribI4bv:
invlpg [rax]
db "glVertexAttribI4bv", 0
ret

global glVertexAttribI4i:function
align 16
glVertexAttribI4i:
invlpg [rax]
db "glVertexAttribI4i", 0
ret

global glVertexAttribI4iv:function
align 16
glVertexAttribI4iv:
invlpg [rax]
db "glVertexAttribI4iv", 0
ret

global glVertexAttribI4sv:function
align 16
glVertexAttribI4sv:
invlpg [rax]
db "glVertexAttribI4sv", 0
ret

global glVertexAttribI4ubv:function
align 16
glVertexAttribI4ubv:
invlpg [rax]
db "glVertexAttribI4ubv", 0
ret

global glVertexAttribI4ui:function
align 16
glVertexAttribI4ui:
invlpg [rax]
db "glVertexAttribI4ui", 0
ret

global glVertexAttribI4uiv:function
align 16
glVertexAttribI4uiv:
invlpg [rax]
db "glVertexAttribI4uiv", 0
ret

global glVertexAttribI4usv:function
align 16
glVertexAttribI4usv:
invlpg [rax]
db "glVertexAttribI4usv", 0
ret

global glVertexAttribIPointer:function
align 16
glVertexAttribIPointer:
invlpg [rax]
db "glVertexAttribIPointer", 0
ret

global glDrawArraysInstanced:function
align 16
glDrawArraysInstanced:
invlpg [rax]
db "glDrawArraysInstanced", 0
ret

global glDrawElementsInstanced:function
align 16
glDrawElementsInstanced:
invlpg [rax]
db "glDrawElementsInstanced", 0
ret

global glPrimitiveRestartIndex:function
align 16
glPrimitiveRestartIndex:
invlpg [rax]
db "glPrimitiveRestartIndex", 0
ret

global glTexBuffer:function
align 16
glTexBuffer:
invlpg [rax]
db "glTexBuffer", 0
ret

global glFramebufferTexture:function
align 16
glFramebufferTexture:
invlpg [rax]
db "glFramebufferTexture", 0
ret

global glGetBufferParameteri64v:function
align 16
glGetBufferParameteri64v:
invlpg [rax]
db "glGetBufferParameteri64v", 0
ret

global glGetInteger64i_v:function
align 16
glGetInteger64i_v:
invlpg [rax]
db "glGetInteger64i_v", 0
ret

global glVertexAttribDivisor:function
align 16
glVertexAttribDivisor:
invlpg [rax]
db "glVertexAttribDivisor", 0
ret

global glBlendEquationSeparatei:function
align 16
glBlendEquationSeparatei:
invlpg [rax]
db "glBlendEquationSeparatei", 0
ret

global glBlendEquationi:function
align 16
glBlendEquationi:
invlpg [rax]
db "glBlendEquationi", 0
ret

global glBlendFuncSeparatei:function
align 16
glBlendFuncSeparatei:
invlpg [rax]
db "glBlendFuncSeparatei", 0
ret

global glBlendFunci:function
align 16
glBlendFunci:
invlpg [rax]
db "glBlendFunci", 0
ret

global glMinSampleShading:function
align 16
glMinSampleShading:
invlpg [rax]
db "glMinSampleShading", 0
ret

global glBindBuffersBase:function
align 16
glBindBuffersBase:
invlpg [rax]
db "glBindBuffersBase", 0
ret

global glBindBuffersRange:function
align 16
glBindBuffersRange:
invlpg [rax]
db "glBindBuffersRange", 0
ret

global glBindImageTextures:function
align 16
glBindImageTextures:
invlpg [rax]
db "glBindImageTextures", 0
ret

global glBindSamplers:function
align 16
glBindSamplers:
invlpg [rax]
db "glBindSamplers", 0
ret

global glBindTextures:function
align 16
glBindTextures:
invlpg [rax]
db "glBindTextures", 0
ret

global glBindVertexBuffers:function
align 16
glBindVertexBuffers:
invlpg [rax]
db "glBindVertexBuffers", 0
ret

global glBufferStorage:function
align 16
glBufferStorage:
invlpg [rax]
db "glBufferStorage", 0
ret

global glClearTexImage:function
align 16
glClearTexImage:
invlpg [rax]
db "glClearTexImage", 0
ret

global glClearTexSubImage:function
align 16
glClearTexSubImage:
invlpg [rax]
db "glClearTexSubImage", 0
ret

global glClipControl:function
align 16
glClipControl:
invlpg [rax]
db "glClipControl", 0
ret

global glCreateTransformFeedbacks:function
align 16
glCreateTransformFeedbacks:
invlpg [rax]
db "glCreateTransformFeedbacks", 0
ret

global glTransformFeedbackBufferBase:function
align 16
glTransformFeedbackBufferBase:
invlpg [rax]
db "glTransformFeedbackBufferBase", 0
ret

global glTransformFeedbackBufferRange:function
align 16
glTransformFeedbackBufferRange:
invlpg [rax]
db "glTransformFeedbackBufferRange", 0
ret

global glGetTransformFeedbackiv:function
align 16
glGetTransformFeedbackiv:
invlpg [rax]
db "glGetTransformFeedbackiv", 0
ret

global glGetTransformFeedbacki_v:function
align 16
glGetTransformFeedbacki_v:
invlpg [rax]
db "glGetTransformFeedbacki_v", 0
ret

global glGetTransformFeedbacki64_v:function
align 16
glGetTransformFeedbacki64_v:
invlpg [rax]
db "glGetTransformFeedbacki64_v", 0
ret

global glCreateBuffers:function
align 16
glCreateBuffers:
invlpg [rax]
db "glCreateBuffers", 0
ret

global glNamedBufferStorage:function
align 16
glNamedBufferStorage:
invlpg [rax]
db "glNamedBufferStorage", 0
ret

global glNamedBufferData:function
align 16
glNamedBufferData:
invlpg [rax]
db "glNamedBufferData", 0
ret

global glNamedBufferSubData:function
align 16
glNamedBufferSubData:
invlpg [rax]
db "glNamedBufferSubData", 0
ret

global glCopyNamedBufferSubData:function
align 16
glCopyNamedBufferSubData:
invlpg [rax]
db "glCopyNamedBufferSubData", 0
ret

global glClearNamedBufferData:function
align 16
glClearNamedBufferData:
invlpg [rax]
db "glClearNamedBufferData", 0
ret

global glClearNamedBufferSubData:function
align 16
glClearNamedBufferSubData:
invlpg [rax]
db "glClearNamedBufferSubData", 0
ret

global glMapNamedBuffer:function
align 16
glMapNamedBuffer:
invlpg [rax]
db "glMapNamedBuffer", 0
ret

global glMapNamedBufferRange:function
align 16
glMapNamedBufferRange:
invlpg [rax]
db "glMapNamedBufferRange", 0
ret

global glUnmapNamedBuffer:function
align 16
glUnmapNamedBuffer:
invlpg [rax]
db "glUnmapNamedBuffer", 0
ret

global glFlushMappedNamedBufferRange:function
align 16
glFlushMappedNamedBufferRange:
invlpg [rax]
db "glFlushMappedNamedBufferRange", 0
ret

global glGetNamedBufferParameteriv:function
align 16
glGetNamedBufferParameteriv:
invlpg [rax]
db "glGetNamedBufferParameteriv", 0
ret

global glGetNamedBufferParameteri64v:function
align 16
glGetNamedBufferParameteri64v:
invlpg [rax]
db "glGetNamedBufferParameteri64v", 0
ret

global glGetNamedBufferPointerv:function
align 16
glGetNamedBufferPointerv:
invlpg [rax]
db "glGetNamedBufferPointerv", 0
ret

global glGetNamedBufferSubData:function
align 16
glGetNamedBufferSubData:
invlpg [rax]
db "glGetNamedBufferSubData", 0
ret

global glCreateFramebuffers:function
align 16
glCreateFramebuffers:
invlpg [rax]
db "glCreateFramebuffers", 0
ret

global glNamedFramebufferRenderbuffer:function
align 16
glNamedFramebufferRenderbuffer:
invlpg [rax]
db "glNamedFramebufferRenderbuffer", 0
ret

global glNamedFramebufferParameteri:function
align 16
glNamedFramebufferParameteri:
invlpg [rax]
db "glNamedFramebufferParameteri", 0
ret

global glNamedFramebufferTexture:function
align 16
glNamedFramebufferTexture:
invlpg [rax]
db "glNamedFramebufferTexture", 0
ret

global glNamedFramebufferTextureLayer:function
align 16
glNamedFramebufferTextureLayer:
invlpg [rax]
db "glNamedFramebufferTextureLayer", 0
ret

global glNamedFramebufferDrawBuffer:function
align 16
glNamedFramebufferDrawBuffer:
invlpg [rax]
db "glNamedFramebufferDrawBuffer", 0
ret

global glNamedFramebufferDrawBuffers:function
align 16
glNamedFramebufferDrawBuffers:
invlpg [rax]
db "glNamedFramebufferDrawBuffers", 0
ret

global glNamedFramebufferReadBuffer:function
align 16
glNamedFramebufferReadBuffer:
invlpg [rax]
db "glNamedFramebufferReadBuffer", 0
ret

global glInvalidateNamedFramebufferData:function
align 16
glInvalidateNamedFramebufferData:
invlpg [rax]
db "glInvalidateNamedFramebufferData", 0
ret

global glInvalidateNamedFramebufferSubData:function
align 16
glInvalidateNamedFramebufferSubData:
invlpg [rax]
db "glInvalidateNamedFramebufferSubData", 0
ret

global glClearNamedFramebufferiv:function
align 16
glClearNamedFramebufferiv:
invlpg [rax]
db "glClearNamedFramebufferiv", 0
ret

global glClearNamedFramebufferuiv:function
align 16
glClearNamedFramebufferuiv:
invlpg [rax]
db "glClearNamedFramebufferuiv", 0
ret

global glClearNamedFramebufferfv:function
align 16
glClearNamedFramebufferfv:
invlpg [rax]
db "glClearNamedFramebufferfv", 0
ret

global glClearNamedFramebufferfi:function
align 16
glClearNamedFramebufferfi:
invlpg [rax]
db "glClearNamedFramebufferfi", 0
ret

global glBlitNamedFramebuffer:function
align 16
glBlitNamedFramebuffer:
invlpg [rax]
db "glBlitNamedFramebuffer", 0
ret

global glCheckNamedFramebufferStatus:function
align 16
glCheckNamedFramebufferStatus:
invlpg [rax]
db "glCheckNamedFramebufferStatus", 0
ret

global glGetNamedFramebufferParameteriv:function
align 16
glGetNamedFramebufferParameteriv:
invlpg [rax]
db "glGetNamedFramebufferParameteriv", 0
ret

global glGetNamedFramebufferAttachmentParameteriv:function
align 16
glGetNamedFramebufferAttachmentParameteriv:
invlpg [rax]
db "glGetNamedFramebufferAttachmentParameteriv", 0
ret

global glCreateRenderbuffers:function
align 16
glCreateRenderbuffers:
invlpg [rax]
db "glCreateRenderbuffers", 0
ret

global glNamedRenderbufferStorage:function
align 16
glNamedRenderbufferStorage:
invlpg [rax]
db "glNamedRenderbufferStorage", 0
ret

global glNamedRenderbufferStorageMultisample:function
align 16
glNamedRenderbufferStorageMultisample:
invlpg [rax]
db "glNamedRenderbufferStorageMultisample", 0
ret

global glGetNamedRenderbufferParameteriv:function
align 16
glGetNamedRenderbufferParameteriv:
invlpg [rax]
db "glGetNamedRenderbufferParameteriv", 0
ret

global glCreateTextures:function
align 16
glCreateTextures:
invlpg [rax]
db "glCreateTextures", 0
ret

global glTextureBuffer:function
align 16
glTextureBuffer:
invlpg [rax]
db "glTextureBuffer", 0
ret

global glTextureBufferRange:function
align 16
glTextureBufferRange:
invlpg [rax]
db "glTextureBufferRange", 0
ret

global glTextureStorage1D:function
align 16
glTextureStorage1D:
invlpg [rax]
db "glTextureStorage1D", 0
ret

global glTextureStorage2D:function
align 16
glTextureStorage2D:
invlpg [rax]
db "glTextureStorage2D", 0
ret

global glTextureStorage3D:function
align 16
glTextureStorage3D:
invlpg [rax]
db "glTextureStorage3D", 0
ret

global glTextureStorage2DMultisample:function
align 16
glTextureStorage2DMultisample:
invlpg [rax]
db "glTextureStorage2DMultisample", 0
ret

global glTextureStorage3DMultisample:function
align 16
glTextureStorage3DMultisample:
invlpg [rax]
db "glTextureStorage3DMultisample", 0
ret

global glTextureSubImage1D:function
align 16
glTextureSubImage1D:
invlpg [rax]
db "glTextureSubImage1D", 0
ret

global glTextureSubImage2D:function
align 16
glTextureSubImage2D:
invlpg [rax]
db "glTextureSubImage2D", 0
ret

global glTextureSubImage3D:function
align 16
glTextureSubImage3D:
invlpg [rax]
db "glTextureSubImage3D", 0
ret

global glCompressedTextureSubImage1D:function
align 16
glCompressedTextureSubImage1D:
invlpg [rax]
db "glCompressedTextureSubImage1D", 0
ret

global glCompressedTextureSubImage2D:function
align 16
glCompressedTextureSubImage2D:
invlpg [rax]
db "glCompressedTextureSubImage2D", 0
ret

global glCompressedTextureSubImage3D:function
align 16
glCompressedTextureSubImage3D:
invlpg [rax]
db "glCompressedTextureSubImage3D", 0
ret

global glCopyTextureSubImage1D:function
align 16
glCopyTextureSubImage1D:
invlpg [rax]
db "glCopyTextureSubImage1D", 0
ret

global glCopyTextureSubImage2D:function
align 16
glCopyTextureSubImage2D:
invlpg [rax]
db "glCopyTextureSubImage2D", 0
ret

global glCopyTextureSubImage3D:function
align 16
glCopyTextureSubImage3D:
invlpg [rax]
db "glCopyTextureSubImage3D", 0
ret

global glTextureParameterf:function
align 16
glTextureParameterf:
invlpg [rax]
db "glTextureParameterf", 0
ret

global glTextureParameterfv:function
align 16
glTextureParameterfv:
invlpg [rax]
db "glTextureParameterfv", 0
ret

global glTextureParameteri:function
align 16
glTextureParameteri:
invlpg [rax]
db "glTextureParameteri", 0
ret

global glTextureParameterIiv:function
align 16
glTextureParameterIiv:
invlpg [rax]
db "glTextureParameterIiv", 0
ret

global glTextureParameterIuiv:function
align 16
glTextureParameterIuiv:
invlpg [rax]
db "glTextureParameterIuiv", 0
ret

global glTextureParameteriv:function
align 16
glTextureParameteriv:
invlpg [rax]
db "glTextureParameteriv", 0
ret

global glGenerateTextureMipmap:function
align 16
glGenerateTextureMipmap:
invlpg [rax]
db "glGenerateTextureMipmap", 0
ret

global glBindTextureUnit:function
align 16
glBindTextureUnit:
invlpg [rax]
db "glBindTextureUnit", 0
ret

global glGetTextureImage:function
align 16
glGetTextureImage:
invlpg [rax]
db "glGetTextureImage", 0
ret

global glGetCompressedTextureImage:function
align 16
glGetCompressedTextureImage:
invlpg [rax]
db "glGetCompressedTextureImage", 0
ret

global glGetTextureLevelParameterfv:function
align 16
glGetTextureLevelParameterfv:
invlpg [rax]
db "glGetTextureLevelParameterfv", 0
ret

global glGetTextureLevelParameteriv:function
align 16
glGetTextureLevelParameteriv:
invlpg [rax]
db "glGetTextureLevelParameteriv", 0
ret

global glGetTextureParameterfv:function
align 16
glGetTextureParameterfv:
invlpg [rax]
db "glGetTextureParameterfv", 0
ret

global glGetTextureParameterIiv:function
align 16
glGetTextureParameterIiv:
invlpg [rax]
db "glGetTextureParameterIiv", 0
ret

global glGetTextureParameterIuiv:function
align 16
glGetTextureParameterIuiv:
invlpg [rax]
db "glGetTextureParameterIuiv", 0
ret

global glGetTextureParameteriv:function
align 16
glGetTextureParameteriv:
invlpg [rax]
db "glGetTextureParameteriv", 0
ret

global glCreateVertexArrays:function
align 16
glCreateVertexArrays:
invlpg [rax]
db "glCreateVertexArrays", 0
ret

global glDisableVertexArrayAttrib:function
align 16
glDisableVertexArrayAttrib:
invlpg [rax]
db "glDisableVertexArrayAttrib", 0
ret

global glEnableVertexArrayAttrib:function
align 16
glEnableVertexArrayAttrib:
invlpg [rax]
db "glEnableVertexArrayAttrib", 0
ret

global glVertexArrayElementBuffer:function
align 16
glVertexArrayElementBuffer:
invlpg [rax]
db "glVertexArrayElementBuffer", 0
ret

global glVertexArrayVertexBuffer:function
align 16
glVertexArrayVertexBuffer:
invlpg [rax]
db "glVertexArrayVertexBuffer", 0
ret

global glVertexArrayVertexBuffers:function
align 16
glVertexArrayVertexBuffers:
invlpg [rax]
db "glVertexArrayVertexBuffers", 0
ret

global glVertexArrayAttribBinding:function
align 16
glVertexArrayAttribBinding:
invlpg [rax]
db "glVertexArrayAttribBinding", 0
ret

global glVertexArrayAttribFormat:function
align 16
glVertexArrayAttribFormat:
invlpg [rax]
db "glVertexArrayAttribFormat", 0
ret

global glVertexArrayAttribIFormat:function
align 16
glVertexArrayAttribIFormat:
invlpg [rax]
db "glVertexArrayAttribIFormat", 0
ret

global glVertexArrayAttribLFormat:function
align 16
glVertexArrayAttribLFormat:
invlpg [rax]
db "glVertexArrayAttribLFormat", 0
ret

global glVertexArrayBindingDivisor:function
align 16
glVertexArrayBindingDivisor:
invlpg [rax]
db "glVertexArrayBindingDivisor", 0
ret

global glGetVertexArrayiv:function
align 16
glGetVertexArrayiv:
invlpg [rax]
db "glGetVertexArrayiv", 0
ret

global glGetVertexArrayIndexediv:function
align 16
glGetVertexArrayIndexediv:
invlpg [rax]
db "glGetVertexArrayIndexediv", 0
ret

global glGetVertexArrayIndexed64iv:function
align 16
glGetVertexArrayIndexed64iv:
invlpg [rax]
db "glGetVertexArrayIndexed64iv", 0
ret

global glCreateSamplers:function
align 16
glCreateSamplers:
invlpg [rax]
db "glCreateSamplers", 0
ret

global glCreateProgramPipelines:function
align 16
glCreateProgramPipelines:
invlpg [rax]
db "glCreateProgramPipelines", 0
ret

global glTextureBarrier:function
align 16
glTextureBarrier:
invlpg [rax]
db "glTextureBarrier", 0
ret

global glCreateQueries:function
align 16
glCreateQueries:
invlpg [rax]
db "glCreateQueries", 0
ret

global glGetQueryBufferObjecti64v:function
align 16
glGetQueryBufferObjecti64v:
invlpg [rax]
db "glGetQueryBufferObjecti64v", 0
ret

global glGetQueryBufferObjectiv:function
align 16
glGetQueryBufferObjectiv:
invlpg [rax]
db "glGetQueryBufferObjectiv", 0
ret

global glGetQueryBufferObjectui64v:function
align 16
glGetQueryBufferObjectui64v:
invlpg [rax]
db "glGetQueryBufferObjectui64v", 0
ret

global glGetQueryBufferObjectuiv:function
align 16
glGetQueryBufferObjectuiv:
invlpg [rax]
db "glGetQueryBufferObjectuiv", 0
ret

global glMemoryBarrierByRegion:function
align 16
glMemoryBarrierByRegion:
invlpg [rax]
db "glMemoryBarrierByRegion", 0
ret

global glGetTextureSubImage:function
align 16
glGetTextureSubImage:
invlpg [rax]
db "glGetTextureSubImage", 0
ret

global glGetCompressedTextureSubImage:function
align 16
glGetCompressedTextureSubImage:
invlpg [rax]
db "glGetCompressedTextureSubImage", 0
ret

global glGetGraphicsResetStatus:function
align 16
glGetGraphicsResetStatus:
invlpg [rax]
db "glGetGraphicsResetStatus", 0
ret

global glGetnUniformfv:function
align 16
glGetnUniformfv:
invlpg [rax]
db "glGetnUniformfv", 0
ret

global glGetnUniformiv:function
align 16
glGetnUniformiv:
invlpg [rax]
db "glGetnUniformiv", 0
ret

global glGetnUniformuiv:function
align 16
glGetnUniformuiv:
invlpg [rax]
db "glGetnUniformuiv", 0
ret

global glReadnPixels:function
align 16
glReadnPixels:
invlpg [rax]
db "glReadnPixels", 0
ret

global glSpecializeShader:function
align 16
glSpecializeShader:
invlpg [rax]
db "glSpecializeShader", 0
ret

global glClearDepthf:function
align 16
glClearDepthf:
invlpg [rax]
db "glClearDepthf", 0
ret

global glDepthRangef:function
align 16
glDepthRangef:
invlpg [rax]
db "glDepthRangef", 0
ret

global glGetShaderPrecisionFormat:function
align 16
glGetShaderPrecisionFormat:
invlpg [rax]
db "glGetShaderPrecisionFormat", 0
ret

global glReleaseShaderCompiler:function
align 16
glReleaseShaderCompiler:
invlpg [rax]
db "glReleaseShaderCompiler", 0
ret

global glShaderBinary:function
align 16
glShaderBinary:
invlpg [rax]
db "glShaderBinary", 0
ret

global glDrawArraysInstancedBaseInstance:function
align 16
glDrawArraysInstancedBaseInstance:
invlpg [rax]
db "glDrawArraysInstancedBaseInstance", 0
ret

global glDrawElementsInstancedBaseInstance:function
align 16
glDrawElementsInstancedBaseInstance:
invlpg [rax]
db "glDrawElementsInstancedBaseInstance", 0
ret

global glDrawElementsInstancedBaseVertexBaseInstance:function
align 16
glDrawElementsInstancedBaseVertexBaseInstance:
invlpg [rax]
db "glDrawElementsInstancedBaseVertexBaseInstance", 0
ret

global glBindFragDataLocationIndexed:function
align 16
glBindFragDataLocationIndexed:
invlpg [rax]
db "glBindFragDataLocationIndexed", 0
ret

global glGetFragDataIndex:function
align 16
glGetFragDataIndex:
invlpg [rax]
db "glGetFragDataIndex", 0
ret

global glCreateSyncFromCLeventARB:function
align 16
glCreateSyncFromCLeventARB:
invlpg [rax]
db "glCreateSyncFromCLeventARB", 0
ret

global glClearBufferData:function
align 16
glClearBufferData:
invlpg [rax]
db "glClearBufferData", 0
ret

global glClearBufferSubData:function
align 16
glClearBufferSubData:
invlpg [rax]
db "glClearBufferSubData", 0
ret

global glClearNamedBufferDataEXT:function
align 16
glClearNamedBufferDataEXT:
invlpg [rax]
db "glClearNamedBufferDataEXT", 0
ret

global glClearNamedBufferSubDataEXT:function
align 16
glClearNamedBufferSubDataEXT:
invlpg [rax]
db "glClearNamedBufferSubDataEXT", 0
ret

global glClampColorARB:function
align 16
glClampColorARB:
invlpg [rax]
db "glClampColorARB", 0
ret

global glDispatchCompute:function
align 16
glDispatchCompute:
invlpg [rax]
db "glDispatchCompute", 0
ret

global glDispatchComputeIndirect:function
align 16
glDispatchComputeIndirect:
invlpg [rax]
db "glDispatchComputeIndirect", 0
ret

global glCopyBufferSubData:function
align 16
glCopyBufferSubData:
invlpg [rax]
db "glCopyBufferSubData", 0
ret

global glCopyImageSubData:function
align 16
glCopyImageSubData:
invlpg [rax]
db "glCopyImageSubData", 0
ret

global glDebugMessageCallback:function
align 16
glDebugMessageCallback:
invlpg [rax]
db "glDebugMessageCallback", 0
ret

global glDebugMessageCallbackARB:function
align 16
glDebugMessageCallbackARB:
invlpg [rax]
db "glDebugMessageCallbackARB", 0
ret

global glDebugMessageControlARB:function
align 16
glDebugMessageControlARB:
invlpg [rax]
db "glDebugMessageControlARB", 0
ret

global glDebugMessageInsertARB:function
align 16
glDebugMessageInsertARB:
invlpg [rax]
db "glDebugMessageInsertARB", 0
ret

global glGetDebugMessageLogARB:function
align 16
glGetDebugMessageLogARB:
invlpg [rax]
db "glGetDebugMessageLogARB", 0
ret

global glDrawBuffersARB:function
align 16
glDrawBuffersARB:
invlpg [rax]
db "glDrawBuffersARB", 0
ret

global glBlendEquationSeparateiARB:function
align 16
glBlendEquationSeparateiARB:
invlpg [rax]
db "glBlendEquationSeparateiARB", 0
ret

global glBlendEquationiARB:function
align 16
glBlendEquationiARB:
invlpg [rax]
db "glBlendEquationiARB", 0
ret

global glBlendFuncSeparateiARB:function
align 16
glBlendFuncSeparateiARB:
invlpg [rax]
db "glBlendFuncSeparateiARB", 0
ret

global glBlendFunciARB:function
align 16
glBlendFunciARB:
invlpg [rax]
db "glBlendFunciARB", 0
ret

global glDrawElementsBaseVertex:function
align 16
glDrawElementsBaseVertex:
invlpg [rax]
db "glDrawElementsBaseVertex", 0
ret

global glDrawElementsInstancedBaseVertex:function
align 16
glDrawElementsInstancedBaseVertex:
invlpg [rax]
db "glDrawElementsInstancedBaseVertex", 0
ret

global glDrawRangeElementsBaseVertex:function
align 16
glDrawRangeElementsBaseVertex:
invlpg [rax]
db "glDrawRangeElementsBaseVertex", 0
ret

global glMultiDrawElementsBaseVertex:function
align 16
glMultiDrawElementsBaseVertex:
invlpg [rax]
db "glMultiDrawElementsBaseVertex", 0
ret

global glDrawArraysIndirect:function
align 16
glDrawArraysIndirect:
invlpg [rax]
db "glDrawArraysIndirect", 0
ret

global glDrawElementsIndirect:function
align 16
glDrawElementsIndirect:
invlpg [rax]
db "glDrawElementsIndirect", 0
ret

global glDrawArraysInstancedARB:function
align 16
glDrawArraysInstancedARB:
invlpg [rax]
db "glDrawArraysInstancedARB", 0
ret

global glDrawElementsInstancedARB:function
align 16
glDrawElementsInstancedARB:
invlpg [rax]
db "glDrawElementsInstancedARB", 0
ret

global glFramebufferParameteri:function
align 16
glFramebufferParameteri:
invlpg [rax]
db "glFramebufferParameteri", 0
ret

global glGetFramebufferParameteriv:function
align 16
glGetFramebufferParameteriv:
invlpg [rax]
db "glGetFramebufferParameteriv", 0
ret

global glGetNamedFramebufferParameterivEXT:function
align 16
glGetNamedFramebufferParameterivEXT:
invlpg [rax]
db "glGetNamedFramebufferParameterivEXT", 0
ret

global glNamedFramebufferParameteriEXT:function
align 16
glNamedFramebufferParameteriEXT:
invlpg [rax]
db "glNamedFramebufferParameteriEXT", 0
ret

global glBindFramebuffer:function
align 16
glBindFramebuffer:
invlpg [rax]
db "glBindFramebuffer", 0
ret

global glBindRenderbuffer:function
align 16
glBindRenderbuffer:
invlpg [rax]
db "glBindRenderbuffer", 0
ret

global glBlitFramebuffer:function
align 16
glBlitFramebuffer:
invlpg [rax]
db "glBlitFramebuffer", 0
ret

global glCheckFramebufferStatus:function
align 16
glCheckFramebufferStatus:
invlpg [rax]
db "glCheckFramebufferStatus", 0
ret

global glDeleteFramebuffers:function
align 16
glDeleteFramebuffers:
invlpg [rax]
db "glDeleteFramebuffers", 0
ret

global glDeleteRenderbuffers:function
align 16
glDeleteRenderbuffers:
invlpg [rax]
db "glDeleteRenderbuffers", 0
ret

global glFramebufferRenderbuffer:function
align 16
glFramebufferRenderbuffer:
invlpg [rax]
db "glFramebufferRenderbuffer", 0
ret

global glFramebufferTexture1D:function
align 16
glFramebufferTexture1D:
invlpg [rax]
db "glFramebufferTexture1D", 0
ret

global glFramebufferTexture2D:function
align 16
glFramebufferTexture2D:
invlpg [rax]
db "glFramebufferTexture2D", 0
ret

global glFramebufferTexture3D:function
align 16
glFramebufferTexture3D:
invlpg [rax]
db "glFramebufferTexture3D", 0
ret

global glFramebufferTextureLayer:function
align 16
glFramebufferTextureLayer:
invlpg [rax]
db "glFramebufferTextureLayer", 0
ret

global glGenFramebuffers:function
align 16
glGenFramebuffers:
invlpg [rax]
db "glGenFramebuffers", 0
ret

global glGenRenderbuffers:function
align 16
glGenRenderbuffers:
invlpg [rax]
db "glGenRenderbuffers", 0
ret

global glGenerateMipmap:function
align 16
glGenerateMipmap:
invlpg [rax]
db "glGenerateMipmap", 0
ret

global glGetFramebufferAttachmentParameteriv:function
align 16
glGetFramebufferAttachmentParameteriv:
invlpg [rax]
db "glGetFramebufferAttachmentParameteriv", 0
ret

global glGetRenderbufferParameteriv:function
align 16
glGetRenderbufferParameteriv:
invlpg [rax]
db "glGetRenderbufferParameteriv", 0
ret

global glIsFramebuffer:function
align 16
glIsFramebuffer:
invlpg [rax]
db "glIsFramebuffer", 0
ret

global glIsRenderbuffer:function
align 16
glIsRenderbuffer:
invlpg [rax]
db "glIsRenderbuffer", 0
ret

global glRenderbufferStorage:function
align 16
glRenderbufferStorage:
invlpg [rax]
db "glRenderbufferStorage", 0
ret

global glRenderbufferStorageMultisample:function
align 16
glRenderbufferStorageMultisample:
invlpg [rax]
db "glRenderbufferStorageMultisample", 0
ret

global glFramebufferTextureARB:function
align 16
glFramebufferTextureARB:
invlpg [rax]
db "glFramebufferTextureARB", 0
ret

global glFramebufferTextureFaceARB:function
align 16
glFramebufferTextureFaceARB:
invlpg [rax]
db "glFramebufferTextureFaceARB", 0
ret

global glFramebufferTextureLayerARB:function
align 16
glFramebufferTextureLayerARB:
invlpg [rax]
db "glFramebufferTextureLayerARB", 0
ret

global glProgramParameteriARB:function
align 16
glProgramParameteriARB:
invlpg [rax]
db "glProgramParameteriARB", 0
ret

global glGetProgramBinary:function
align 16
glGetProgramBinary:
invlpg [rax]
db "glGetProgramBinary", 0
ret

global glProgramBinary:function
align 16
glProgramBinary:
invlpg [rax]
db "glProgramBinary", 0
ret

global glProgramParameteri:function
align 16
glProgramParameteri:
invlpg [rax]
db "glProgramParameteri", 0
ret

global glGetUniformdv:function
align 16
glGetUniformdv:
invlpg [rax]
db "glGetUniformdv", 0
ret

global glUniform1d:function
align 16
glUniform1d:
invlpg [rax]
db "glUniform1d", 0
ret

global glUniform1dv:function
align 16
glUniform1dv:
invlpg [rax]
db "glUniform1dv", 0
ret

global glUniform2d:function
align 16
glUniform2d:
invlpg [rax]
db "glUniform2d", 0
ret

global glUniform2dv:function
align 16
glUniform2dv:
invlpg [rax]
db "glUniform2dv", 0
ret

global glUniform3d:function
align 16
glUniform3d:
invlpg [rax]
db "glUniform3d", 0
ret

global glUniform3dv:function
align 16
glUniform3dv:
invlpg [rax]
db "glUniform3dv", 0
ret

global glUniform4d:function
align 16
glUniform4d:
invlpg [rax]
db "glUniform4d", 0
ret

global glUniform4dv:function
align 16
glUniform4dv:
invlpg [rax]
db "glUniform4dv", 0
ret

global glUniformMatrix2dv:function
align 16
glUniformMatrix2dv:
invlpg [rax]
db "glUniformMatrix2dv", 0
ret

global glUniformMatrix2x3dv:function
align 16
glUniformMatrix2x3dv:
invlpg [rax]
db "glUniformMatrix2x3dv", 0
ret

global glUniformMatrix2x4dv:function
align 16
glUniformMatrix2x4dv:
invlpg [rax]
db "glUniformMatrix2x4dv", 0
ret

global glUniformMatrix3dv:function
align 16
glUniformMatrix3dv:
invlpg [rax]
db "glUniformMatrix3dv", 0
ret

global glUniformMatrix3x2dv:function
align 16
glUniformMatrix3x2dv:
invlpg [rax]
db "glUniformMatrix3x2dv", 0
ret

global glUniformMatrix3x4dv:function
align 16
glUniformMatrix3x4dv:
invlpg [rax]
db "glUniformMatrix3x4dv", 0
ret

global glUniformMatrix4dv:function
align 16
glUniformMatrix4dv:
invlpg [rax]
db "glUniformMatrix4dv", 0
ret

global glUniformMatrix4x2dv:function
align 16
glUniformMatrix4x2dv:
invlpg [rax]
db "glUniformMatrix4x2dv", 0
ret

global glUniformMatrix4x3dv:function
align 16
glUniformMatrix4x3dv:
invlpg [rax]
db "glUniformMatrix4x3dv", 0
ret

global glVertexAttribDivisorARB:function
align 16
glVertexAttribDivisorARB:
invlpg [rax]
db "glVertexAttribDivisorARB", 0
ret

global glGetInternalformativ:function
align 16
glGetInternalformativ:
invlpg [rax]
db "glGetInternalformativ", 0
ret

global glGetInternalformati64v:function
align 16
glGetInternalformati64v:
invlpg [rax]
db "glGetInternalformati64v", 0
ret

global glInvalidateBufferData:function
align 16
glInvalidateBufferData:
invlpg [rax]
db "glInvalidateBufferData", 0
ret

global glInvalidateBufferSubData:function
align 16
glInvalidateBufferSubData:
invlpg [rax]
db "glInvalidateBufferSubData", 0
ret

global glInvalidateFramebuffer:function
align 16
glInvalidateFramebuffer:
invlpg [rax]
db "glInvalidateFramebuffer", 0
ret

global glInvalidateSubFramebuffer:function
align 16
glInvalidateSubFramebuffer:
invlpg [rax]
db "glInvalidateSubFramebuffer", 0
ret

global glInvalidateTexImage:function
align 16
glInvalidateTexImage:
invlpg [rax]
db "glInvalidateTexImage", 0
ret

global glInvalidateTexSubImage:function
align 16
glInvalidateTexSubImage:
invlpg [rax]
db "glInvalidateTexSubImage", 0
ret

global glFlushMappedBufferRange:function
align 16
glFlushMappedBufferRange:
invlpg [rax]
db "glFlushMappedBufferRange", 0
ret

global glMapBufferRange:function
align 16
glMapBufferRange:
invlpg [rax]
db "glMapBufferRange", 0
ret

global glCurrentPaletteMatrixARB:function
align 16
glCurrentPaletteMatrixARB:
invlpg [rax]
db "glCurrentPaletteMatrixARB", 0
ret

global glMatrixIndexPointerARB:function
align 16
glMatrixIndexPointerARB:
invlpg [rax]
db "glMatrixIndexPointerARB", 0
ret

global glMatrixIndexubvARB:function
align 16
glMatrixIndexubvARB:
invlpg [rax]
db "glMatrixIndexubvARB", 0
ret

global glMatrixIndexuivARB:function
align 16
glMatrixIndexuivARB:
invlpg [rax]
db "glMatrixIndexuivARB", 0
ret

global glMatrixIndexusvARB:function
align 16
glMatrixIndexusvARB:
invlpg [rax]
db "glMatrixIndexusvARB", 0
ret

global glMultiDrawArraysIndirect:function
align 16
glMultiDrawArraysIndirect:
invlpg [rax]
db "glMultiDrawArraysIndirect", 0
ret

global glMultiDrawElementsIndirect:function
align 16
glMultiDrawElementsIndirect:
invlpg [rax]
db "glMultiDrawElementsIndirect", 0
ret

global glSampleCoverageARB:function
align 16
glSampleCoverageARB:
invlpg [rax]
db "glSampleCoverageARB", 0
ret

global glActiveTextureARB:function
align 16
glActiveTextureARB:
invlpg [rax]
db "glActiveTextureARB", 0
ret

global glClientActiveTextureARB:function
align 16
glClientActiveTextureARB:
invlpg [rax]
db "glClientActiveTextureARB", 0
ret

global glMultiTexCoord1dARB:function
align 16
glMultiTexCoord1dARB:
invlpg [rax]
db "glMultiTexCoord1dARB", 0
ret

global glMultiTexCoord1dvARB:function
align 16
glMultiTexCoord1dvARB:
invlpg [rax]
db "glMultiTexCoord1dvARB", 0
ret

global glMultiTexCoord1fARB:function
align 16
glMultiTexCoord1fARB:
invlpg [rax]
db "glMultiTexCoord1fARB", 0
ret

global glMultiTexCoord1fvARB:function
align 16
glMultiTexCoord1fvARB:
invlpg [rax]
db "glMultiTexCoord1fvARB", 0
ret

global glMultiTexCoord1iARB:function
align 16
glMultiTexCoord1iARB:
invlpg [rax]
db "glMultiTexCoord1iARB", 0
ret

global glMultiTexCoord1ivARB:function
align 16
glMultiTexCoord1ivARB:
invlpg [rax]
db "glMultiTexCoord1ivARB", 0
ret

global glMultiTexCoord1sARB:function
align 16
glMultiTexCoord1sARB:
invlpg [rax]
db "glMultiTexCoord1sARB", 0
ret

global glMultiTexCoord1svARB:function
align 16
glMultiTexCoord1svARB:
invlpg [rax]
db "glMultiTexCoord1svARB", 0
ret

global glMultiTexCoord2dARB:function
align 16
glMultiTexCoord2dARB:
invlpg [rax]
db "glMultiTexCoord2dARB", 0
ret

global glMultiTexCoord2dvARB:function
align 16
glMultiTexCoord2dvARB:
invlpg [rax]
db "glMultiTexCoord2dvARB", 0
ret

global glMultiTexCoord2fARB:function
align 16
glMultiTexCoord2fARB:
invlpg [rax]
db "glMultiTexCoord2fARB", 0
ret

global glMultiTexCoord2fvARB:function
align 16
glMultiTexCoord2fvARB:
invlpg [rax]
db "glMultiTexCoord2fvARB", 0
ret

global glMultiTexCoord2iARB:function
align 16
glMultiTexCoord2iARB:
invlpg [rax]
db "glMultiTexCoord2iARB", 0
ret

global glMultiTexCoord2ivARB:function
align 16
glMultiTexCoord2ivARB:
invlpg [rax]
db "glMultiTexCoord2ivARB", 0
ret

global glMultiTexCoord2sARB:function
align 16
glMultiTexCoord2sARB:
invlpg [rax]
db "glMultiTexCoord2sARB", 0
ret

global glMultiTexCoord2svARB:function
align 16
glMultiTexCoord2svARB:
invlpg [rax]
db "glMultiTexCoord2svARB", 0
ret

global glMultiTexCoord3dARB:function
align 16
glMultiTexCoord3dARB:
invlpg [rax]
db "glMultiTexCoord3dARB", 0
ret

global glMultiTexCoord3dvARB:function
align 16
glMultiTexCoord3dvARB:
invlpg [rax]
db "glMultiTexCoord3dvARB", 0
ret

global glMultiTexCoord3fARB:function
align 16
glMultiTexCoord3fARB:
invlpg [rax]
db "glMultiTexCoord3fARB", 0
ret

global glMultiTexCoord3fvARB:function
align 16
glMultiTexCoord3fvARB:
invlpg [rax]
db "glMultiTexCoord3fvARB", 0
ret

global glMultiTexCoord3iARB:function
align 16
glMultiTexCoord3iARB:
invlpg [rax]
db "glMultiTexCoord3iARB", 0
ret

global glMultiTexCoord3ivARB:function
align 16
glMultiTexCoord3ivARB:
invlpg [rax]
db "glMultiTexCoord3ivARB", 0
ret

global glMultiTexCoord3sARB:function
align 16
glMultiTexCoord3sARB:
invlpg [rax]
db "glMultiTexCoord3sARB", 0
ret

global glMultiTexCoord3svARB:function
align 16
glMultiTexCoord3svARB:
invlpg [rax]
db "glMultiTexCoord3svARB", 0
ret

global glMultiTexCoord4dARB:function
align 16
glMultiTexCoord4dARB:
invlpg [rax]
db "glMultiTexCoord4dARB", 0
ret

global glMultiTexCoord4dvARB:function
align 16
glMultiTexCoord4dvARB:
invlpg [rax]
db "glMultiTexCoord4dvARB", 0
ret

global glMultiTexCoord4fARB:function
align 16
glMultiTexCoord4fARB:
invlpg [rax]
db "glMultiTexCoord4fARB", 0
ret

global glMultiTexCoord4fvARB:function
align 16
glMultiTexCoord4fvARB:
invlpg [rax]
db "glMultiTexCoord4fvARB", 0
ret

global glMultiTexCoord4iARB:function
align 16
glMultiTexCoord4iARB:
invlpg [rax]
db "glMultiTexCoord4iARB", 0
ret

global glMultiTexCoord4ivARB:function
align 16
glMultiTexCoord4ivARB:
invlpg [rax]
db "glMultiTexCoord4ivARB", 0
ret

global glMultiTexCoord4sARB:function
align 16
glMultiTexCoord4sARB:
invlpg [rax]
db "glMultiTexCoord4sARB", 0
ret

global glMultiTexCoord4svARB:function
align 16
glMultiTexCoord4svARB:
invlpg [rax]
db "glMultiTexCoord4svARB", 0
ret

global glBeginQueryARB:function
align 16
glBeginQueryARB:
invlpg [rax]
db "glBeginQueryARB", 0
ret

global glDeleteQueriesARB:function
align 16
glDeleteQueriesARB:
invlpg [rax]
db "glDeleteQueriesARB", 0
ret

global glEndQueryARB:function
align 16
glEndQueryARB:
invlpg [rax]
db "glEndQueryARB", 0
ret

global glGenQueriesARB:function
align 16
glGenQueriesARB:
invlpg [rax]
db "glGenQueriesARB", 0
ret

global glGetQueryObjectivARB:function
align 16
glGetQueryObjectivARB:
invlpg [rax]
db "glGetQueryObjectivARB", 0
ret

global glGetQueryObjectuivARB:function
align 16
glGetQueryObjectuivARB:
invlpg [rax]
db "glGetQueryObjectuivARB", 0
ret

global glGetQueryivARB:function
align 16
glGetQueryivARB:
invlpg [rax]
db "glGetQueryivARB", 0
ret

global glIsQueryARB:function
align 16
glIsQueryARB:
invlpg [rax]
db "glIsQueryARB", 0
ret

global glPointParameterfARB:function
align 16
glPointParameterfARB:
invlpg [rax]
db "glPointParameterfARB", 0
ret

global glPointParameterfvARB:function
align 16
glPointParameterfvARB:
invlpg [rax]
db "glPointParameterfvARB", 0
ret

global glGetProgramInterfaceiv:function
align 16
glGetProgramInterfaceiv:
invlpg [rax]
db "glGetProgramInterfaceiv", 0
ret

global glGetProgramResourceIndex:function
align 16
glGetProgramResourceIndex:
invlpg [rax]
db "glGetProgramResourceIndex", 0
ret

global glGetProgramResourceLocation:function
align 16
glGetProgramResourceLocation:
invlpg [rax]
db "glGetProgramResourceLocation", 0
ret

global glGetProgramResourceLocationIndex:function
align 16
glGetProgramResourceLocationIndex:
invlpg [rax]
db "glGetProgramResourceLocationIndex", 0
ret

global glGetProgramResourceName:function
align 16
glGetProgramResourceName:
invlpg [rax]
db "glGetProgramResourceName", 0
ret

global glGetProgramResourceiv:function
align 16
glGetProgramResourceiv:
invlpg [rax]
db "glGetProgramResourceiv", 0
ret

global glProvokingVertex:function
align 16
glProvokingVertex:
invlpg [rax]
db "glProvokingVertex", 0
ret

global glGetGraphicsResetStatusARB:function
align 16
glGetGraphicsResetStatusARB:
invlpg [rax]
db "glGetGraphicsResetStatusARB", 0
ret

global glGetnColorTableARB:function
align 16
glGetnColorTableARB:
invlpg [rax]
db "glGetnColorTableARB", 0
ret

global glGetnCompressedTexImageARB:function
align 16
glGetnCompressedTexImageARB:
invlpg [rax]
db "glGetnCompressedTexImageARB", 0
ret

global glGetnConvolutionFilterARB:function
align 16
glGetnConvolutionFilterARB:
invlpg [rax]
db "glGetnConvolutionFilterARB", 0
ret

global glGetnHistogramARB:function
align 16
glGetnHistogramARB:
invlpg [rax]
db "glGetnHistogramARB", 0
ret

global glGetnMapdvARB:function
align 16
glGetnMapdvARB:
invlpg [rax]
db "glGetnMapdvARB", 0
ret

global glGetnMapfvARB:function
align 16
glGetnMapfvARB:
invlpg [rax]
db "glGetnMapfvARB", 0
ret

global glGetnMapivARB:function
align 16
glGetnMapivARB:
invlpg [rax]
db "glGetnMapivARB", 0
ret

global glGetnMinmaxARB:function
align 16
glGetnMinmaxARB:
invlpg [rax]
db "glGetnMinmaxARB", 0
ret

global glGetnPixelMapfvARB:function
align 16
glGetnPixelMapfvARB:
invlpg [rax]
db "glGetnPixelMapfvARB", 0
ret

global glGetnPixelMapuivARB:function
align 16
glGetnPixelMapuivARB:
invlpg [rax]
db "glGetnPixelMapuivARB", 0
ret

global glGetnPixelMapusvARB:function
align 16
glGetnPixelMapusvARB:
invlpg [rax]
db "glGetnPixelMapusvARB", 0
ret

global glGetnPolygonStippleARB:function
align 16
glGetnPolygonStippleARB:
invlpg [rax]
db "glGetnPolygonStippleARB", 0
ret

global glGetnSeparableFilterARB:function
align 16
glGetnSeparableFilterARB:
invlpg [rax]
db "glGetnSeparableFilterARB", 0
ret

global glGetnTexImageARB:function
align 16
glGetnTexImageARB:
invlpg [rax]
db "glGetnTexImageARB", 0
ret

global glGetnUniformdvARB:function
align 16
glGetnUniformdvARB:
invlpg [rax]
db "glGetnUniformdvARB", 0
ret

global glGetnUniformfvARB:function
align 16
glGetnUniformfvARB:
invlpg [rax]
db "glGetnUniformfvARB", 0
ret

global glGetnUniformivARB:function
align 16
glGetnUniformivARB:
invlpg [rax]
db "glGetnUniformivARB", 0
ret

global glGetnUniformuivARB:function
align 16
glGetnUniformuivARB:
invlpg [rax]
db "glGetnUniformuivARB", 0
ret

global glReadnPixelsARB:function
align 16
glReadnPixelsARB:
invlpg [rax]
db "glReadnPixelsARB", 0
ret

global glMinSampleShadingARB:function
align 16
glMinSampleShadingARB:
invlpg [rax]
db "glMinSampleShadingARB", 0
ret

global glBindSampler:function
align 16
glBindSampler:
invlpg [rax]
db "glBindSampler", 0
ret

global glDeleteSamplers:function
align 16
glDeleteSamplers:
invlpg [rax]
db "glDeleteSamplers", 0
ret

global glGenSamplers:function
align 16
glGenSamplers:
invlpg [rax]
db "glGenSamplers", 0
ret

global glGetSamplerParameterIiv:function
align 16
glGetSamplerParameterIiv:
invlpg [rax]
db "glGetSamplerParameterIiv", 0
ret

global glGetSamplerParameterIuiv:function
align 16
glGetSamplerParameterIuiv:
invlpg [rax]
db "glGetSamplerParameterIuiv", 0
ret

global glGetSamplerParameterfv:function
align 16
glGetSamplerParameterfv:
invlpg [rax]
db "glGetSamplerParameterfv", 0
ret

global glGetSamplerParameteriv:function
align 16
glGetSamplerParameteriv:
invlpg [rax]
db "glGetSamplerParameteriv", 0
ret

global glIsSampler:function
align 16
glIsSampler:
invlpg [rax]
db "glIsSampler", 0
ret

global glSamplerParameterIiv:function
align 16
glSamplerParameterIiv:
invlpg [rax]
db "glSamplerParameterIiv", 0
ret

global glSamplerParameterIuiv:function
align 16
glSamplerParameterIuiv:
invlpg [rax]
db "glSamplerParameterIuiv", 0
ret

global glSamplerParameterf:function
align 16
glSamplerParameterf:
invlpg [rax]
db "glSamplerParameterf", 0
ret

global glSamplerParameterfv:function
align 16
glSamplerParameterfv:
invlpg [rax]
db "glSamplerParameterfv", 0
ret

global glSamplerParameteri:function
align 16
glSamplerParameteri:
invlpg [rax]
db "glSamplerParameteri", 0
ret

global glSamplerParameteriv:function
align 16
glSamplerParameteriv:
invlpg [rax]
db "glSamplerParameteriv", 0
ret

global glActiveShaderProgram:function
align 16
glActiveShaderProgram:
invlpg [rax]
db "glActiveShaderProgram", 0
ret

global glBindProgramPipeline:function
align 16
glBindProgramPipeline:
invlpg [rax]
db "glBindProgramPipeline", 0
ret

global glCreateShaderProgramv:function
align 16
glCreateShaderProgramv:
invlpg [rax]
db "glCreateShaderProgramv", 0
ret

global glDeleteProgramPipelines:function
align 16
glDeleteProgramPipelines:
invlpg [rax]
db "glDeleteProgramPipelines", 0
ret

global glGenProgramPipelines:function
align 16
glGenProgramPipelines:
invlpg [rax]
db "glGenProgramPipelines", 0
ret

global glGetProgramPipelineInfoLog:function
align 16
glGetProgramPipelineInfoLog:
invlpg [rax]
db "glGetProgramPipelineInfoLog", 0
ret

global glGetProgramPipelineiv:function
align 16
glGetProgramPipelineiv:
invlpg [rax]
db "glGetProgramPipelineiv", 0
ret

global glIsProgramPipeline:function
align 16
glIsProgramPipeline:
invlpg [rax]
db "glIsProgramPipeline", 0
ret

global glProgramUniform1d:function
align 16
glProgramUniform1d:
invlpg [rax]
db "glProgramUniform1d", 0
ret

global glProgramUniform1dv:function
align 16
glProgramUniform1dv:
invlpg [rax]
db "glProgramUniform1dv", 0
ret

global glProgramUniform1f:function
align 16
glProgramUniform1f:
invlpg [rax]
db "glProgramUniform1f", 0
ret

global glProgramUniform1fv:function
align 16
glProgramUniform1fv:
invlpg [rax]
db "glProgramUniform1fv", 0
ret

global glProgramUniform1i:function
align 16
glProgramUniform1i:
invlpg [rax]
db "glProgramUniform1i", 0
ret

global glProgramUniform1iv:function
align 16
glProgramUniform1iv:
invlpg [rax]
db "glProgramUniform1iv", 0
ret

global glProgramUniform1ui:function
align 16
glProgramUniform1ui:
invlpg [rax]
db "glProgramUniform1ui", 0
ret

global glProgramUniform1uiv:function
align 16
glProgramUniform1uiv:
invlpg [rax]
db "glProgramUniform1uiv", 0
ret

global glProgramUniform2d:function
align 16
glProgramUniform2d:
invlpg [rax]
db "glProgramUniform2d", 0
ret

global glProgramUniform2dv:function
align 16
glProgramUniform2dv:
invlpg [rax]
db "glProgramUniform2dv", 0
ret

global glProgramUniform2f:function
align 16
glProgramUniform2f:
invlpg [rax]
db "glProgramUniform2f", 0
ret

global glProgramUniform2fv:function
align 16
glProgramUniform2fv:
invlpg [rax]
db "glProgramUniform2fv", 0
ret

global glProgramUniform2i:function
align 16
glProgramUniform2i:
invlpg [rax]
db "glProgramUniform2i", 0
ret

global glProgramUniform2iv:function
align 16
glProgramUniform2iv:
invlpg [rax]
db "glProgramUniform2iv", 0
ret

global glProgramUniform2ui:function
align 16
glProgramUniform2ui:
invlpg [rax]
db "glProgramUniform2ui", 0
ret

global glProgramUniform2uiv:function
align 16
glProgramUniform2uiv:
invlpg [rax]
db "glProgramUniform2uiv", 0
ret

global glProgramUniform3d:function
align 16
glProgramUniform3d:
invlpg [rax]
db "glProgramUniform3d", 0
ret

global glProgramUniform3dv:function
align 16
glProgramUniform3dv:
invlpg [rax]
db "glProgramUniform3dv", 0
ret

global glProgramUniform3f:function
align 16
glProgramUniform3f:
invlpg [rax]
db "glProgramUniform3f", 0
ret

global glProgramUniform3fv:function
align 16
glProgramUniform3fv:
invlpg [rax]
db "glProgramUniform3fv", 0
ret

global glProgramUniform3i:function
align 16
glProgramUniform3i:
invlpg [rax]
db "glProgramUniform3i", 0
ret

global glProgramUniform3iv:function
align 16
glProgramUniform3iv:
invlpg [rax]
db "glProgramUniform3iv", 0
ret

global glProgramUniform3ui:function
align 16
glProgramUniform3ui:
invlpg [rax]
db "glProgramUniform3ui", 0
ret

global glProgramUniform3uiv:function
align 16
glProgramUniform3uiv:
invlpg [rax]
db "glProgramUniform3uiv", 0
ret

global glProgramUniform4d:function
align 16
glProgramUniform4d:
invlpg [rax]
db "glProgramUniform4d", 0
ret

global glProgramUniform4dv:function
align 16
glProgramUniform4dv:
invlpg [rax]
db "glProgramUniform4dv", 0
ret

global glProgramUniform4f:function
align 16
glProgramUniform4f:
invlpg [rax]
db "glProgramUniform4f", 0
ret

global glProgramUniform4fv:function
align 16
glProgramUniform4fv:
invlpg [rax]
db "glProgramUniform4fv", 0
ret

global glProgramUniform4i:function
align 16
glProgramUniform4i:
invlpg [rax]
db "glProgramUniform4i", 0
ret

global glProgramUniform4iv:function
align 16
glProgramUniform4iv:
invlpg [rax]
db "glProgramUniform4iv", 0
ret

global glProgramUniform4ui:function
align 16
glProgramUniform4ui:
invlpg [rax]
db "glProgramUniform4ui", 0
ret

global glProgramUniform4uiv:function
align 16
glProgramUniform4uiv:
invlpg [rax]
db "glProgramUniform4uiv", 0
ret

global glProgramUniformMatrix2dv:function
align 16
glProgramUniformMatrix2dv:
invlpg [rax]
db "glProgramUniformMatrix2dv", 0
ret

global glProgramUniformMatrix2fv:function
align 16
glProgramUniformMatrix2fv:
invlpg [rax]
db "glProgramUniformMatrix2fv", 0
ret

global glProgramUniformMatrix2x3dv:function
align 16
glProgramUniformMatrix2x3dv:
invlpg [rax]
db "glProgramUniformMatrix2x3dv", 0
ret

global glProgramUniformMatrix2x3fv:function
align 16
glProgramUniformMatrix2x3fv:
invlpg [rax]
db "glProgramUniformMatrix2x3fv", 0
ret

global glProgramUniformMatrix2x4dv:function
align 16
glProgramUniformMatrix2x4dv:
invlpg [rax]
db "glProgramUniformMatrix2x4dv", 0
ret

global glProgramUniformMatrix2x4fv:function
align 16
glProgramUniformMatrix2x4fv:
invlpg [rax]
db "glProgramUniformMatrix2x4fv", 0
ret

global glProgramUniformMatrix3dv:function
align 16
glProgramUniformMatrix3dv:
invlpg [rax]
db "glProgramUniformMatrix3dv", 0
ret

global glProgramUniformMatrix3fv:function
align 16
glProgramUniformMatrix3fv:
invlpg [rax]
db "glProgramUniformMatrix3fv", 0
ret

global glProgramUniformMatrix3x2dv:function
align 16
glProgramUniformMatrix3x2dv:
invlpg [rax]
db "glProgramUniformMatrix3x2dv", 0
ret

global glProgramUniformMatrix3x2fv:function
align 16
glProgramUniformMatrix3x2fv:
invlpg [rax]
db "glProgramUniformMatrix3x2fv", 0
ret

global glProgramUniformMatrix3x4dv:function
align 16
glProgramUniformMatrix3x4dv:
invlpg [rax]
db "glProgramUniformMatrix3x4dv", 0
ret

global glProgramUniformMatrix3x4fv:function
align 16
glProgramUniformMatrix3x4fv:
invlpg [rax]
db "glProgramUniformMatrix3x4fv", 0
ret

global glProgramUniformMatrix4dv:function
align 16
glProgramUniformMatrix4dv:
invlpg [rax]
db "glProgramUniformMatrix4dv", 0
ret

global glProgramUniformMatrix4fv:function
align 16
glProgramUniformMatrix4fv:
invlpg [rax]
db "glProgramUniformMatrix4fv", 0
ret

global glProgramUniformMatrix4x2dv:function
align 16
glProgramUniformMatrix4x2dv:
invlpg [rax]
db "glProgramUniformMatrix4x2dv", 0
ret

global glProgramUniformMatrix4x2fv:function
align 16
glProgramUniformMatrix4x2fv:
invlpg [rax]
db "glProgramUniformMatrix4x2fv", 0
ret

global glProgramUniformMatrix4x3dv:function
align 16
glProgramUniformMatrix4x3dv:
invlpg [rax]
db "glProgramUniformMatrix4x3dv", 0
ret

global glProgramUniformMatrix4x3fv:function
align 16
glProgramUniformMatrix4x3fv:
invlpg [rax]
db "glProgramUniformMatrix4x3fv", 0
ret

global glUseProgramStages:function
align 16
glUseProgramStages:
invlpg [rax]
db "glUseProgramStages", 0
ret

global glValidateProgramPipeline:function
align 16
glValidateProgramPipeline:
invlpg [rax]
db "glValidateProgramPipeline", 0
ret

global glGetActiveAtomicCounterBufferiv:function
align 16
glGetActiveAtomicCounterBufferiv:
invlpg [rax]
db "glGetActiveAtomicCounterBufferiv", 0
ret

global glBindImageTexture:function
align 16
glBindImageTexture:
invlpg [rax]
db "glBindImageTexture", 0
ret

global glMemoryBarrier:function
align 16
glMemoryBarrier:
invlpg [rax]
db "glMemoryBarrier", 0
ret

global glAttachObjectARB:function
align 16
glAttachObjectARB:
invlpg [rax]
db "glAttachObjectARB", 0
ret

global glCompileShaderARB:function
align 16
glCompileShaderARB:
invlpg [rax]
db "glCompileShaderARB", 0
ret

global glCreateProgramObjectARB:function
align 16
glCreateProgramObjectARB:
invlpg [rax]
db "glCreateProgramObjectARB", 0
ret

global glCreateShaderObjectARB:function
align 16
glCreateShaderObjectARB:
invlpg [rax]
db "glCreateShaderObjectARB", 0
ret

global glDeleteObjectARB:function
align 16
glDeleteObjectARB:
invlpg [rax]
db "glDeleteObjectARB", 0
ret

global glDetachObjectARB:function
align 16
glDetachObjectARB:
invlpg [rax]
db "glDetachObjectARB", 0
ret

global glGetActiveUniformARB:function
align 16
glGetActiveUniformARB:
invlpg [rax]
db "glGetActiveUniformARB", 0
ret

global glGetAttachedObjectsARB:function
align 16
glGetAttachedObjectsARB:
invlpg [rax]
db "glGetAttachedObjectsARB", 0
ret

global glGetHandleARB:function
align 16
glGetHandleARB:
invlpg [rax]
db "glGetHandleARB", 0
ret

global glGetInfoLogARB:function
align 16
glGetInfoLogARB:
invlpg [rax]
db "glGetInfoLogARB", 0
ret

global glGetObjectParameterfvARB:function
align 16
glGetObjectParameterfvARB:
invlpg [rax]
db "glGetObjectParameterfvARB", 0
ret

global glGetObjectParameterivARB:function
align 16
glGetObjectParameterivARB:
invlpg [rax]
db "glGetObjectParameterivARB", 0
ret

global glGetShaderSourceARB:function
align 16
glGetShaderSourceARB:
invlpg [rax]
db "glGetShaderSourceARB", 0
ret

global glGetUniformLocationARB:function
align 16
glGetUniformLocationARB:
invlpg [rax]
db "glGetUniformLocationARB", 0
ret

global glGetUniformfvARB:function
align 16
glGetUniformfvARB:
invlpg [rax]
db "glGetUniformfvARB", 0
ret

global glGetUniformivARB:function
align 16
glGetUniformivARB:
invlpg [rax]
db "glGetUniformivARB", 0
ret

global glLinkProgramARB:function
align 16
glLinkProgramARB:
invlpg [rax]
db "glLinkProgramARB", 0
ret

global glShaderSourceARB:function
align 16
glShaderSourceARB:
invlpg [rax]
db "glShaderSourceARB", 0
ret

global glUniform1fARB:function
align 16
glUniform1fARB:
invlpg [rax]
db "glUniform1fARB", 0
ret

global glUniform1fvARB:function
align 16
glUniform1fvARB:
invlpg [rax]
db "glUniform1fvARB", 0
ret

global glUniform1iARB:function
align 16
glUniform1iARB:
invlpg [rax]
db "glUniform1iARB", 0
ret

global glUniform1ivARB:function
align 16
glUniform1ivARB:
invlpg [rax]
db "glUniform1ivARB", 0
ret

global glUniform2fARB:function
align 16
glUniform2fARB:
invlpg [rax]
db "glUniform2fARB", 0
ret

global glUniform2fvARB:function
align 16
glUniform2fvARB:
invlpg [rax]
db "glUniform2fvARB", 0
ret

global glUniform2iARB:function
align 16
glUniform2iARB:
invlpg [rax]
db "glUniform2iARB", 0
ret

global glUniform2ivARB:function
align 16
glUniform2ivARB:
invlpg [rax]
db "glUniform2ivARB", 0
ret

global glUniform3fARB:function
align 16
glUniform3fARB:
invlpg [rax]
db "glUniform3fARB", 0
ret

global glUniform3fvARB:function
align 16
glUniform3fvARB:
invlpg [rax]
db "glUniform3fvARB", 0
ret

global glUniform3iARB:function
align 16
glUniform3iARB:
invlpg [rax]
db "glUniform3iARB", 0
ret

global glUniform3ivARB:function
align 16
glUniform3ivARB:
invlpg [rax]
db "glUniform3ivARB", 0
ret

global glUniform4fARB:function
align 16
glUniform4fARB:
invlpg [rax]
db "glUniform4fARB", 0
ret

global glUniform4fvARB:function
align 16
glUniform4fvARB:
invlpg [rax]
db "glUniform4fvARB", 0
ret

global glUniform4iARB:function
align 16
glUniform4iARB:
invlpg [rax]
db "glUniform4iARB", 0
ret

global glUniform4ivARB:function
align 16
glUniform4ivARB:
invlpg [rax]
db "glUniform4ivARB", 0
ret

global glUniformMatrix2fvARB:function
align 16
glUniformMatrix2fvARB:
invlpg [rax]
db "glUniformMatrix2fvARB", 0
ret

global glUniformMatrix3fvARB:function
align 16
glUniformMatrix3fvARB:
invlpg [rax]
db "glUniformMatrix3fvARB", 0
ret

global glUniformMatrix4fvARB:function
align 16
glUniformMatrix4fvARB:
invlpg [rax]
db "glUniformMatrix4fvARB", 0
ret

global glUseProgramObjectARB:function
align 16
glUseProgramObjectARB:
invlpg [rax]
db "glUseProgramObjectARB", 0
ret

global glValidateProgramARB:function
align 16
glValidateProgramARB:
invlpg [rax]
db "glValidateProgramARB", 0
ret

global glShaderStorageBlockBinding:function
align 16
glShaderStorageBlockBinding:
invlpg [rax]
db "glShaderStorageBlockBinding", 0
ret

global glGetActiveSubroutineName:function
align 16
glGetActiveSubroutineName:
invlpg [rax]
db "glGetActiveSubroutineName", 0
ret

global glGetActiveSubroutineUniformName:function
align 16
glGetActiveSubroutineUniformName:
invlpg [rax]
db "glGetActiveSubroutineUniformName", 0
ret

global glGetActiveSubroutineUniformiv:function
align 16
glGetActiveSubroutineUniformiv:
invlpg [rax]
db "glGetActiveSubroutineUniformiv", 0
ret

global glGetProgramStageiv:function
align 16
glGetProgramStageiv:
invlpg [rax]
db "glGetProgramStageiv", 0
ret

global glGetSubroutineIndex:function
align 16
glGetSubroutineIndex:
invlpg [rax]
db "glGetSubroutineIndex", 0
ret

global glGetSubroutineUniformLocation:function
align 16
glGetSubroutineUniformLocation:
invlpg [rax]
db "glGetSubroutineUniformLocation", 0
ret

global glGetUniformSubroutineuiv:function
align 16
glGetUniformSubroutineuiv:
invlpg [rax]
db "glGetUniformSubroutineuiv", 0
ret

global glUniformSubroutinesuiv:function
align 16
glUniformSubroutinesuiv:
invlpg [rax]
db "glUniformSubroutinesuiv", 0
ret

global glCompileShaderIncludeARB:function
align 16
glCompileShaderIncludeARB:
invlpg [rax]
db "glCompileShaderIncludeARB", 0
ret

global glDeleteNamedStringARB:function
align 16
glDeleteNamedStringARB:
invlpg [rax]
db "glDeleteNamedStringARB", 0
ret

global glGetNamedStringARB:function
align 16
glGetNamedStringARB:
invlpg [rax]
db "glGetNamedStringARB", 0
ret

global glGetNamedStringivARB:function
align 16
glGetNamedStringivARB:
invlpg [rax]
db "glGetNamedStringivARB", 0
ret

global glIsNamedStringARB:function
align 16
glIsNamedStringARB:
invlpg [rax]
db "glIsNamedStringARB", 0
ret

global glNamedStringARB:function
align 16
glNamedStringARB:
invlpg [rax]
db "glNamedStringARB", 0
ret

global glClientWaitSync:function
align 16
glClientWaitSync:
invlpg [rax]
db "glClientWaitSync", 0
ret

global glDeleteSync:function
align 16
glDeleteSync:
invlpg [rax]
db "glDeleteSync", 0
ret

global glFenceSync:function
align 16
glFenceSync:
invlpg [rax]
db "glFenceSync", 0
ret

global glGetInteger64v:function
align 16
glGetInteger64v:
invlpg [rax]
db "glGetInteger64v", 0
ret

global glGetSynciv:function
align 16
glGetSynciv:
invlpg [rax]
db "glGetSynciv", 0
ret

global glIsSync:function
align 16
glIsSync:
invlpg [rax]
db "glIsSync", 0
ret

global glWaitSync:function
align 16
glWaitSync:
invlpg [rax]
db "glWaitSync", 0
ret

global glPatchParameterfv:function
align 16
glPatchParameterfv:
invlpg [rax]
db "glPatchParameterfv", 0
ret

global glPatchParameteri:function
align 16
glPatchParameteri:
invlpg [rax]
db "glPatchParameteri", 0
ret

global glTexBufferARB:function
align 16
glTexBufferARB:
invlpg [rax]
db "glTexBufferARB", 0
ret

global glTexBufferRange:function
align 16
glTexBufferRange:
invlpg [rax]
db "glTexBufferRange", 0
ret

global glTextureBufferRangeEXT:function
align 16
glTextureBufferRangeEXT:
invlpg [rax]
db "glTextureBufferRangeEXT", 0
ret

global glCompressedTexImage1DARB:function
align 16
glCompressedTexImage1DARB:
invlpg [rax]
db "glCompressedTexImage1DARB", 0
ret

global glCompressedTexImage2DARB:function
align 16
glCompressedTexImage2DARB:
invlpg [rax]
db "glCompressedTexImage2DARB", 0
ret

global glCompressedTexImage3DARB:function
align 16
glCompressedTexImage3DARB:
invlpg [rax]
db "glCompressedTexImage3DARB", 0
ret

global glCompressedTexSubImage1DARB:function
align 16
glCompressedTexSubImage1DARB:
invlpg [rax]
db "glCompressedTexSubImage1DARB", 0
ret

global glCompressedTexSubImage2DARB:function
align 16
glCompressedTexSubImage2DARB:
invlpg [rax]
db "glCompressedTexSubImage2DARB", 0
ret

global glCompressedTexSubImage3DARB:function
align 16
glCompressedTexSubImage3DARB:
invlpg [rax]
db "glCompressedTexSubImage3DARB", 0
ret

global glGetCompressedTexImageARB:function
align 16
glGetCompressedTexImageARB:
invlpg [rax]
db "glGetCompressedTexImageARB", 0
ret

global glGetMultisamplefv:function
align 16
glGetMultisamplefv:
invlpg [rax]
db "glGetMultisamplefv", 0
ret

global glSampleMaski:function
align 16
glSampleMaski:
invlpg [rax]
db "glSampleMaski", 0
ret

global glTexImage2DMultisample:function
align 16
glTexImage2DMultisample:
invlpg [rax]
db "glTexImage2DMultisample", 0
ret

global glTexImage3DMultisample:function
align 16
glTexImage3DMultisample:
invlpg [rax]
db "glTexImage3DMultisample", 0
ret

global glTexStorage1D:function
align 16
glTexStorage1D:
invlpg [rax]
db "glTexStorage1D", 0
ret

global glTexStorage2D:function
align 16
glTexStorage2D:
invlpg [rax]
db "glTexStorage2D", 0
ret

global glTexStorage3D:function
align 16
glTexStorage3D:
invlpg [rax]
db "glTexStorage3D", 0
ret

global glTextureStorage1DEXT:function
align 16
glTextureStorage1DEXT:
invlpg [rax]
db "glTextureStorage1DEXT", 0
ret

global glTextureStorage2DEXT:function
align 16
glTextureStorage2DEXT:
invlpg [rax]
db "glTextureStorage2DEXT", 0
ret

global glTextureStorage3DEXT:function
align 16
glTextureStorage3DEXT:
invlpg [rax]
db "glTextureStorage3DEXT", 0
ret

global glTexStorage2DMultisample:function
align 16
glTexStorage2DMultisample:
invlpg [rax]
db "glTexStorage2DMultisample", 0
ret

global glTexStorage3DMultisample:function
align 16
glTexStorage3DMultisample:
invlpg [rax]
db "glTexStorage3DMultisample", 0
ret

global glTextureStorage2DMultisampleEXT:function
align 16
glTextureStorage2DMultisampleEXT:
invlpg [rax]
db "glTextureStorage2DMultisampleEXT", 0
ret

global glTextureStorage3DMultisampleEXT:function
align 16
glTextureStorage3DMultisampleEXT:
invlpg [rax]
db "glTextureStorage3DMultisampleEXT", 0
ret

global glTextureView:function
align 16
glTextureView:
invlpg [rax]
db "glTextureView", 0
ret

global glGetQueryObjecti64v:function
align 16
glGetQueryObjecti64v:
invlpg [rax]
db "glGetQueryObjecti64v", 0
ret

global glGetQueryObjectui64v:function
align 16
glGetQueryObjectui64v:
invlpg [rax]
db "glGetQueryObjectui64v", 0
ret

global glQueryCounter:function
align 16
glQueryCounter:
invlpg [rax]
db "glQueryCounter", 0
ret

global glBindTransformFeedback:function
align 16
glBindTransformFeedback:
invlpg [rax]
db "glBindTransformFeedback", 0
ret

global glDeleteTransformFeedbacks:function
align 16
glDeleteTransformFeedbacks:
invlpg [rax]
db "glDeleteTransformFeedbacks", 0
ret

global glDrawTransformFeedback:function
align 16
glDrawTransformFeedback:
invlpg [rax]
db "glDrawTransformFeedback", 0
ret

global glGenTransformFeedbacks:function
align 16
glGenTransformFeedbacks:
invlpg [rax]
db "glGenTransformFeedbacks", 0
ret

global glIsTransformFeedback:function
align 16
glIsTransformFeedback:
invlpg [rax]
db "glIsTransformFeedback", 0
ret

global glPauseTransformFeedback:function
align 16
glPauseTransformFeedback:
invlpg [rax]
db "glPauseTransformFeedback", 0
ret

global glResumeTransformFeedback:function
align 16
glResumeTransformFeedback:
invlpg [rax]
db "glResumeTransformFeedback", 0
ret

global glBeginQueryIndexed:function
align 16
glBeginQueryIndexed:
invlpg [rax]
db "glBeginQueryIndexed", 0
ret

global glDrawTransformFeedbackStream:function
align 16
glDrawTransformFeedbackStream:
invlpg [rax]
db "glDrawTransformFeedbackStream", 0
ret

global glEndQueryIndexed:function
align 16
glEndQueryIndexed:
invlpg [rax]
db "glEndQueryIndexed", 0
ret

global glGetQueryIndexediv:function
align 16
glGetQueryIndexediv:
invlpg [rax]
db "glGetQueryIndexediv", 0
ret

global glDrawTransformFeedbackInstanced:function
align 16
glDrawTransformFeedbackInstanced:
invlpg [rax]
db "glDrawTransformFeedbackInstanced", 0
ret

global glDrawTransformFeedbackStreamInstanced:function
align 16
glDrawTransformFeedbackStreamInstanced:
invlpg [rax]
db "glDrawTransformFeedbackStreamInstanced", 0
ret

global glLoadTransposeMatrixdARB:function
align 16
glLoadTransposeMatrixdARB:
invlpg [rax]
db "glLoadTransposeMatrixdARB", 0
ret

global glLoadTransposeMatrixfARB:function
align 16
glLoadTransposeMatrixfARB:
invlpg [rax]
db "glLoadTransposeMatrixfARB", 0
ret

global glMultTransposeMatrixdARB:function
align 16
glMultTransposeMatrixdARB:
invlpg [rax]
db "glMultTransposeMatrixdARB", 0
ret

global glMultTransposeMatrixfARB:function
align 16
glMultTransposeMatrixfARB:
invlpg [rax]
db "glMultTransposeMatrixfARB", 0
ret

global glGetActiveUniformBlockName:function
align 16
glGetActiveUniformBlockName:
invlpg [rax]
db "glGetActiveUniformBlockName", 0
ret

global glGetActiveUniformBlockiv:function
align 16
glGetActiveUniformBlockiv:
invlpg [rax]
db "glGetActiveUniformBlockiv", 0
ret

global glGetActiveUniformName:function
align 16
glGetActiveUniformName:
invlpg [rax]
db "glGetActiveUniformName", 0
ret

global glGetActiveUniformsiv:function
align 16
glGetActiveUniformsiv:
invlpg [rax]
db "glGetActiveUniformsiv", 0
ret

global glGetUniformBlockIndex:function
align 16
glGetUniformBlockIndex:
invlpg [rax]
db "glGetUniformBlockIndex", 0
ret

global glGetUniformIndices:function
align 16
glGetUniformIndices:
invlpg [rax]
db "glGetUniformIndices", 0
ret

global glUniformBlockBinding:function
align 16
glUniformBlockBinding:
invlpg [rax]
db "glUniformBlockBinding", 0
ret

global glBindVertexArray:function
align 16
glBindVertexArray:
invlpg [rax]
db "glBindVertexArray", 0
ret

global glDeleteVertexArrays:function
align 16
glDeleteVertexArrays:
invlpg [rax]
db "glDeleteVertexArrays", 0
ret

global glGenVertexArrays:function
align 16
glGenVertexArrays:
invlpg [rax]
db "glGenVertexArrays", 0
ret

global glIsVertexArray:function
align 16
glIsVertexArray:
invlpg [rax]
db "glIsVertexArray", 0
ret

global glGetVertexAttribLdv:function
align 16
glGetVertexAttribLdv:
invlpg [rax]
db "glGetVertexAttribLdv", 0
ret

global glVertexAttribL1d:function
align 16
glVertexAttribL1d:
invlpg [rax]
db "glVertexAttribL1d", 0
ret

global glVertexAttribL1dv:function
align 16
glVertexAttribL1dv:
invlpg [rax]
db "glVertexAttribL1dv", 0
ret

global glVertexAttribL2d:function
align 16
glVertexAttribL2d:
invlpg [rax]
db "glVertexAttribL2d", 0
ret

global glVertexAttribL2dv:function
align 16
glVertexAttribL2dv:
invlpg [rax]
db "glVertexAttribL2dv", 0
ret

global glVertexAttribL3d:function
align 16
glVertexAttribL3d:
invlpg [rax]
db "glVertexAttribL3d", 0
ret

global glVertexAttribL3dv:function
align 16
glVertexAttribL3dv:
invlpg [rax]
db "glVertexAttribL3dv", 0
ret

global glVertexAttribL4d:function
align 16
glVertexAttribL4d:
invlpg [rax]
db "glVertexAttribL4d", 0
ret

global glVertexAttribL4dv:function
align 16
glVertexAttribL4dv:
invlpg [rax]
db "glVertexAttribL4dv", 0
ret

global glVertexAttribLPointer:function
align 16
glVertexAttribLPointer:
invlpg [rax]
db "glVertexAttribLPointer", 0
ret

global glBindVertexBuffer:function
align 16
glBindVertexBuffer:
invlpg [rax]
db "glBindVertexBuffer", 0
ret

global glVertexArrayBindVertexBufferEXT:function
align 16
glVertexArrayBindVertexBufferEXT:
invlpg [rax]
db "glVertexArrayBindVertexBufferEXT", 0
ret

global glVertexArrayVertexAttribBindingEXT:function
align 16
glVertexArrayVertexAttribBindingEXT:
invlpg [rax]
db "glVertexArrayVertexAttribBindingEXT", 0
ret

global glVertexArrayVertexAttribFormatEXT:function
align 16
glVertexArrayVertexAttribFormatEXT:
invlpg [rax]
db "glVertexArrayVertexAttribFormatEXT", 0
ret

global glVertexArrayVertexAttribIFormatEXT:function
align 16
glVertexArrayVertexAttribIFormatEXT:
invlpg [rax]
db "glVertexArrayVertexAttribIFormatEXT", 0
ret

global glVertexArrayVertexAttribLFormatEXT:function
align 16
glVertexArrayVertexAttribLFormatEXT:
invlpg [rax]
db "glVertexArrayVertexAttribLFormatEXT", 0
ret

global glVertexArrayVertexBindingDivisorEXT:function
align 16
glVertexArrayVertexBindingDivisorEXT:
invlpg [rax]
db "glVertexArrayVertexBindingDivisorEXT", 0
ret

global glVertexAttribBinding:function
align 16
glVertexAttribBinding:
invlpg [rax]
db "glVertexAttribBinding", 0
ret

global glVertexAttribFormat:function
align 16
glVertexAttribFormat:
invlpg [rax]
db "glVertexAttribFormat", 0
ret

global glVertexAttribIFormat:function
align 16
glVertexAttribIFormat:
invlpg [rax]
db "glVertexAttribIFormat", 0
ret

global glVertexAttribLFormat:function
align 16
glVertexAttribLFormat:
invlpg [rax]
db "glVertexAttribLFormat", 0
ret

global glVertexBindingDivisor:function
align 16
glVertexBindingDivisor:
invlpg [rax]
db "glVertexBindingDivisor", 0
ret

global glVertexBlendARB:function
align 16
glVertexBlendARB:
invlpg [rax]
db "glVertexBlendARB", 0
ret

global glWeightPointerARB:function
align 16
glWeightPointerARB:
invlpg [rax]
db "glWeightPointerARB", 0
ret

global glWeightbvARB:function
align 16
glWeightbvARB:
invlpg [rax]
db "glWeightbvARB", 0
ret

global glWeightdvARB:function
align 16
glWeightdvARB:
invlpg [rax]
db "glWeightdvARB", 0
ret

global glWeightfvARB:function
align 16
glWeightfvARB:
invlpg [rax]
db "glWeightfvARB", 0
ret

global glWeightivARB:function
align 16
glWeightivARB:
invlpg [rax]
db "glWeightivARB", 0
ret

global glWeightsvARB:function
align 16
glWeightsvARB:
invlpg [rax]
db "glWeightsvARB", 0
ret

global glWeightubvARB:function
align 16
glWeightubvARB:
invlpg [rax]
db "glWeightubvARB", 0
ret

global glWeightuivARB:function
align 16
glWeightuivARB:
invlpg [rax]
db "glWeightuivARB", 0
ret

global glWeightusvARB:function
align 16
glWeightusvARB:
invlpg [rax]
db "glWeightusvARB", 0
ret

global glBindBufferARB:function
align 16
glBindBufferARB:
invlpg [rax]
db "glBindBufferARB", 0
ret

global glBufferDataARB:function
align 16
glBufferDataARB:
invlpg [rax]
db "glBufferDataARB", 0
ret

global glBufferSubDataARB:function
align 16
glBufferSubDataARB:
invlpg [rax]
db "glBufferSubDataARB", 0
ret

global glDeleteBuffersARB:function
align 16
glDeleteBuffersARB:
invlpg [rax]
db "glDeleteBuffersARB", 0
ret

global glGenBuffersARB:function
align 16
glGenBuffersARB:
invlpg [rax]
db "glGenBuffersARB", 0
ret

global glGetBufferParameterivARB:function
align 16
glGetBufferParameterivARB:
invlpg [rax]
db "glGetBufferParameterivARB", 0
ret

global glGetBufferPointervARB:function
align 16
glGetBufferPointervARB:
invlpg [rax]
db "glGetBufferPointervARB", 0
ret

global glGetBufferSubDataARB:function
align 16
glGetBufferSubDataARB:
invlpg [rax]
db "glGetBufferSubDataARB", 0
ret

global glIsBufferARB:function
align 16
glIsBufferARB:
invlpg [rax]
db "glIsBufferARB", 0
ret

global glMapBufferARB:function
align 16
glMapBufferARB:
invlpg [rax]
db "glMapBufferARB", 0
ret

global glUnmapBufferARB:function
align 16
glUnmapBufferARB:
invlpg [rax]
db "glUnmapBufferARB", 0
ret

global glBindProgramARB:function
align 16
glBindProgramARB:
invlpg [rax]
db "glBindProgramARB", 0
ret

global glDeleteProgramsARB:function
align 16
glDeleteProgramsARB:
invlpg [rax]
db "glDeleteProgramsARB", 0
ret

global glDisableVertexAttribArrayARB:function
align 16
glDisableVertexAttribArrayARB:
invlpg [rax]
db "glDisableVertexAttribArrayARB", 0
ret

global glEnableVertexAttribArrayARB:function
align 16
glEnableVertexAttribArrayARB:
invlpg [rax]
db "glEnableVertexAttribArrayARB", 0
ret

global glGenProgramsARB:function
align 16
glGenProgramsARB:
invlpg [rax]
db "glGenProgramsARB", 0
ret

global glGetProgramEnvParameterdvARB:function
align 16
glGetProgramEnvParameterdvARB:
invlpg [rax]
db "glGetProgramEnvParameterdvARB", 0
ret

global glGetProgramEnvParameterfvARB:function
align 16
glGetProgramEnvParameterfvARB:
invlpg [rax]
db "glGetProgramEnvParameterfvARB", 0
ret

global glGetProgramLocalParameterdvARB:function
align 16
glGetProgramLocalParameterdvARB:
invlpg [rax]
db "glGetProgramLocalParameterdvARB", 0
ret

global glGetProgramLocalParameterfvARB:function
align 16
glGetProgramLocalParameterfvARB:
invlpg [rax]
db "glGetProgramLocalParameterfvARB", 0
ret

global glGetProgramStringARB:function
align 16
glGetProgramStringARB:
invlpg [rax]
db "glGetProgramStringARB", 0
ret

global glGetProgramivARB:function
align 16
glGetProgramivARB:
invlpg [rax]
db "glGetProgramivARB", 0
ret

global glGetVertexAttribPointervARB:function
align 16
glGetVertexAttribPointervARB:
invlpg [rax]
db "glGetVertexAttribPointervARB", 0
ret

global glGetVertexAttribdvARB:function
align 16
glGetVertexAttribdvARB:
invlpg [rax]
db "glGetVertexAttribdvARB", 0
ret

global glGetVertexAttribfvARB:function
align 16
glGetVertexAttribfvARB:
invlpg [rax]
db "glGetVertexAttribfvARB", 0
ret

global glGetVertexAttribivARB:function
align 16
glGetVertexAttribivARB:
invlpg [rax]
db "glGetVertexAttribivARB", 0
ret

global glIsProgramARB:function
align 16
glIsProgramARB:
invlpg [rax]
db "glIsProgramARB", 0
ret

global glProgramEnvParameter4dARB:function
align 16
glProgramEnvParameter4dARB:
invlpg [rax]
db "glProgramEnvParameter4dARB", 0
ret

global glProgramEnvParameter4dvARB:function
align 16
glProgramEnvParameter4dvARB:
invlpg [rax]
db "glProgramEnvParameter4dvARB", 0
ret

global glProgramEnvParameter4fARB:function
align 16
glProgramEnvParameter4fARB:
invlpg [rax]
db "glProgramEnvParameter4fARB", 0
ret

global glProgramEnvParameter4fvARB:function
align 16
glProgramEnvParameter4fvARB:
invlpg [rax]
db "glProgramEnvParameter4fvARB", 0
ret

global glProgramLocalParameter4dARB:function
align 16
glProgramLocalParameter4dARB:
invlpg [rax]
db "glProgramLocalParameter4dARB", 0
ret

global glProgramLocalParameter4dvARB:function
align 16
glProgramLocalParameter4dvARB:
invlpg [rax]
db "glProgramLocalParameter4dvARB", 0
ret

global glProgramLocalParameter4fARB:function
align 16
glProgramLocalParameter4fARB:
invlpg [rax]
db "glProgramLocalParameter4fARB", 0
ret

global glProgramLocalParameter4fvARB:function
align 16
glProgramLocalParameter4fvARB:
invlpg [rax]
db "glProgramLocalParameter4fvARB", 0
ret

global glProgramStringARB:function
align 16
glProgramStringARB:
invlpg [rax]
db "glProgramStringARB", 0
ret

global glVertexAttrib1dARB:function
align 16
glVertexAttrib1dARB:
invlpg [rax]
db "glVertexAttrib1dARB", 0
ret

global glVertexAttrib1dvARB:function
align 16
glVertexAttrib1dvARB:
invlpg [rax]
db "glVertexAttrib1dvARB", 0
ret

global glVertexAttrib1fARB:function
align 16
glVertexAttrib1fARB:
invlpg [rax]
db "glVertexAttrib1fARB", 0
ret

global glVertexAttrib1fvARB:function
align 16
glVertexAttrib1fvARB:
invlpg [rax]
db "glVertexAttrib1fvARB", 0
ret

global glVertexAttrib1sARB:function
align 16
glVertexAttrib1sARB:
invlpg [rax]
db "glVertexAttrib1sARB", 0
ret

global glVertexAttrib1svARB:function
align 16
glVertexAttrib1svARB:
invlpg [rax]
db "glVertexAttrib1svARB", 0
ret

global glVertexAttrib2dARB:function
align 16
glVertexAttrib2dARB:
invlpg [rax]
db "glVertexAttrib2dARB", 0
ret

global glVertexAttrib2dvARB:function
align 16
glVertexAttrib2dvARB:
invlpg [rax]
db "glVertexAttrib2dvARB", 0
ret

global glVertexAttrib2fARB:function
align 16
glVertexAttrib2fARB:
invlpg [rax]
db "glVertexAttrib2fARB", 0
ret

global glVertexAttrib2fvARB:function
align 16
glVertexAttrib2fvARB:
invlpg [rax]
db "glVertexAttrib2fvARB", 0
ret

global glVertexAttrib2sARB:function
align 16
glVertexAttrib2sARB:
invlpg [rax]
db "glVertexAttrib2sARB", 0
ret

global glVertexAttrib2svARB:function
align 16
glVertexAttrib2svARB:
invlpg [rax]
db "glVertexAttrib2svARB", 0
ret

global glVertexAttrib3dARB:function
align 16
glVertexAttrib3dARB:
invlpg [rax]
db "glVertexAttrib3dARB", 0
ret

global glVertexAttrib3dvARB:function
align 16
glVertexAttrib3dvARB:
invlpg [rax]
db "glVertexAttrib3dvARB", 0
ret

global glVertexAttrib3fARB:function
align 16
glVertexAttrib3fARB:
invlpg [rax]
db "glVertexAttrib3fARB", 0
ret

global glVertexAttrib3fvARB:function
align 16
glVertexAttrib3fvARB:
invlpg [rax]
db "glVertexAttrib3fvARB", 0
ret

global glVertexAttrib3sARB:function
align 16
glVertexAttrib3sARB:
invlpg [rax]
db "glVertexAttrib3sARB", 0
ret

global glVertexAttrib3svARB:function
align 16
glVertexAttrib3svARB:
invlpg [rax]
db "glVertexAttrib3svARB", 0
ret

global glVertexAttrib4NbvARB:function
align 16
glVertexAttrib4NbvARB:
invlpg [rax]
db "glVertexAttrib4NbvARB", 0
ret

global glVertexAttrib4NivARB:function
align 16
glVertexAttrib4NivARB:
invlpg [rax]
db "glVertexAttrib4NivARB", 0
ret

global glVertexAttrib4NsvARB:function
align 16
glVertexAttrib4NsvARB:
invlpg [rax]
db "glVertexAttrib4NsvARB", 0
ret

global glVertexAttrib4NubARB:function
align 16
glVertexAttrib4NubARB:
invlpg [rax]
db "glVertexAttrib4NubARB", 0
ret

global glVertexAttrib4NubvARB:function
align 16
glVertexAttrib4NubvARB:
invlpg [rax]
db "glVertexAttrib4NubvARB", 0
ret

global glVertexAttrib4NuivARB:function
align 16
glVertexAttrib4NuivARB:
invlpg [rax]
db "glVertexAttrib4NuivARB", 0
ret

global glVertexAttrib4NusvARB:function
align 16
glVertexAttrib4NusvARB:
invlpg [rax]
db "glVertexAttrib4NusvARB", 0
ret

global glVertexAttrib4bvARB:function
align 16
glVertexAttrib4bvARB:
invlpg [rax]
db "glVertexAttrib4bvARB", 0
ret

global glVertexAttrib4dARB:function
align 16
glVertexAttrib4dARB:
invlpg [rax]
db "glVertexAttrib4dARB", 0
ret

global glVertexAttrib4dvARB:function
align 16
glVertexAttrib4dvARB:
invlpg [rax]
db "glVertexAttrib4dvARB", 0
ret

global glVertexAttrib4fARB:function
align 16
glVertexAttrib4fARB:
invlpg [rax]
db "glVertexAttrib4fARB", 0
ret

global glVertexAttrib4fvARB:function
align 16
glVertexAttrib4fvARB:
invlpg [rax]
db "glVertexAttrib4fvARB", 0
ret

global glVertexAttrib4ivARB:function
align 16
glVertexAttrib4ivARB:
invlpg [rax]
db "glVertexAttrib4ivARB", 0
ret

global glVertexAttrib4sARB:function
align 16
glVertexAttrib4sARB:
invlpg [rax]
db "glVertexAttrib4sARB", 0
ret

global glVertexAttrib4svARB:function
align 16
glVertexAttrib4svARB:
invlpg [rax]
db "glVertexAttrib4svARB", 0
ret

global glVertexAttrib4ubvARB:function
align 16
glVertexAttrib4ubvARB:
invlpg [rax]
db "glVertexAttrib4ubvARB", 0
ret

global glVertexAttrib4uivARB:function
align 16
glVertexAttrib4uivARB:
invlpg [rax]
db "glVertexAttrib4uivARB", 0
ret

global glVertexAttrib4usvARB:function
align 16
glVertexAttrib4usvARB:
invlpg [rax]
db "glVertexAttrib4usvARB", 0
ret

global glVertexAttribPointerARB:function
align 16
glVertexAttribPointerARB:
invlpg [rax]
db "glVertexAttribPointerARB", 0
ret

global glBindAttribLocationARB:function
align 16
glBindAttribLocationARB:
invlpg [rax]
db "glBindAttribLocationARB", 0
ret

global glGetActiveAttribARB:function
align 16
glGetActiveAttribARB:
invlpg [rax]
db "glGetActiveAttribARB", 0
ret

global glGetAttribLocationARB:function
align 16
glGetAttribLocationARB:
invlpg [rax]
db "glGetAttribLocationARB", 0
ret

global glColorP3ui:function
align 16
glColorP3ui:
invlpg [rax]
db "glColorP3ui", 0
ret

global glColorP3uiv:function
align 16
glColorP3uiv:
invlpg [rax]
db "glColorP3uiv", 0
ret

global glColorP4ui:function
align 16
glColorP4ui:
invlpg [rax]
db "glColorP4ui", 0
ret

global glColorP4uiv:function
align 16
glColorP4uiv:
invlpg [rax]
db "glColorP4uiv", 0
ret

global glMultiTexCoordP1ui:function
align 16
glMultiTexCoordP1ui:
invlpg [rax]
db "glMultiTexCoordP1ui", 0
ret

global glMultiTexCoordP1uiv:function
align 16
glMultiTexCoordP1uiv:
invlpg [rax]
db "glMultiTexCoordP1uiv", 0
ret

global glMultiTexCoordP2ui:function
align 16
glMultiTexCoordP2ui:
invlpg [rax]
db "glMultiTexCoordP2ui", 0
ret

global glMultiTexCoordP2uiv:function
align 16
glMultiTexCoordP2uiv:
invlpg [rax]
db "glMultiTexCoordP2uiv", 0
ret

global glMultiTexCoordP3ui:function
align 16
glMultiTexCoordP3ui:
invlpg [rax]
db "glMultiTexCoordP3ui", 0
ret

global glMultiTexCoordP3uiv:function
align 16
glMultiTexCoordP3uiv:
invlpg [rax]
db "glMultiTexCoordP3uiv", 0
ret

global glMultiTexCoordP4ui:function
align 16
glMultiTexCoordP4ui:
invlpg [rax]
db "glMultiTexCoordP4ui", 0
ret

global glMultiTexCoordP4uiv:function
align 16
glMultiTexCoordP4uiv:
invlpg [rax]
db "glMultiTexCoordP4uiv", 0
ret

global glNormalP3ui:function
align 16
glNormalP3ui:
invlpg [rax]
db "glNormalP3ui", 0
ret

global glNormalP3uiv:function
align 16
glNormalP3uiv:
invlpg [rax]
db "glNormalP3uiv", 0
ret

global glSecondaryColorP3ui:function
align 16
glSecondaryColorP3ui:
invlpg [rax]
db "glSecondaryColorP3ui", 0
ret

global glSecondaryColorP3uiv:function
align 16
glSecondaryColorP3uiv:
invlpg [rax]
db "glSecondaryColorP3uiv", 0
ret

global glTexCoordP1ui:function
align 16
glTexCoordP1ui:
invlpg [rax]
db "glTexCoordP1ui", 0
ret

global glTexCoordP1uiv:function
align 16
glTexCoordP1uiv:
invlpg [rax]
db "glTexCoordP1uiv", 0
ret

global glTexCoordP2ui:function
align 16
glTexCoordP2ui:
invlpg [rax]
db "glTexCoordP2ui", 0
ret

global glTexCoordP2uiv:function
align 16
glTexCoordP2uiv:
invlpg [rax]
db "glTexCoordP2uiv", 0
ret

global glTexCoordP3ui:function
align 16
glTexCoordP3ui:
invlpg [rax]
db "glTexCoordP3ui", 0
ret

global glTexCoordP3uiv:function
align 16
glTexCoordP3uiv:
invlpg [rax]
db "glTexCoordP3uiv", 0
ret

global glTexCoordP4ui:function
align 16
glTexCoordP4ui:
invlpg [rax]
db "glTexCoordP4ui", 0
ret

global glTexCoordP4uiv:function
align 16
glTexCoordP4uiv:
invlpg [rax]
db "glTexCoordP4uiv", 0
ret

global glVertexAttribP1ui:function
align 16
glVertexAttribP1ui:
invlpg [rax]
db "glVertexAttribP1ui", 0
ret

global glVertexAttribP1uiv:function
align 16
glVertexAttribP1uiv:
invlpg [rax]
db "glVertexAttribP1uiv", 0
ret

global glVertexAttribP2ui:function
align 16
glVertexAttribP2ui:
invlpg [rax]
db "glVertexAttribP2ui", 0
ret

global glVertexAttribP2uiv:function
align 16
glVertexAttribP2uiv:
invlpg [rax]
db "glVertexAttribP2uiv", 0
ret

global glVertexAttribP3ui:function
align 16
glVertexAttribP3ui:
invlpg [rax]
db "glVertexAttribP3ui", 0
ret

global glVertexAttribP3uiv:function
align 16
glVertexAttribP3uiv:
invlpg [rax]
db "glVertexAttribP3uiv", 0
ret

global glVertexAttribP4ui:function
align 16
glVertexAttribP4ui:
invlpg [rax]
db "glVertexAttribP4ui", 0
ret

global glVertexAttribP4uiv:function
align 16
glVertexAttribP4uiv:
invlpg [rax]
db "glVertexAttribP4uiv", 0
ret

global glVertexP2ui:function
align 16
glVertexP2ui:
invlpg [rax]
db "glVertexP2ui", 0
ret

global glVertexP2uiv:function
align 16
glVertexP2uiv:
invlpg [rax]
db "glVertexP2uiv", 0
ret

global glVertexP3ui:function
align 16
glVertexP3ui:
invlpg [rax]
db "glVertexP3ui", 0
ret

global glVertexP3uiv:function
align 16
glVertexP3uiv:
invlpg [rax]
db "glVertexP3uiv", 0
ret

global glVertexP4ui:function
align 16
glVertexP4ui:
invlpg [rax]
db "glVertexP4ui", 0
ret

global glVertexP4uiv:function
align 16
glVertexP4uiv:
invlpg [rax]
db "glVertexP4uiv", 0
ret

global glDepthRangeArrayv:function
align 16
glDepthRangeArrayv:
invlpg [rax]
db "glDepthRangeArrayv", 0
ret

global glDepthRangeIndexed:function
align 16
glDepthRangeIndexed:
invlpg [rax]
db "glDepthRangeIndexed", 0
ret

global glGetDoublei_v:function
align 16
glGetDoublei_v:
invlpg [rax]
db "glGetDoublei_v", 0
ret

global glGetFloati_v:function
align 16
glGetFloati_v:
invlpg [rax]
db "glGetFloati_v", 0
ret

global glScissorArrayv:function
align 16
glScissorArrayv:
invlpg [rax]
db "glScissorArrayv", 0
ret

global glScissorIndexed:function
align 16
glScissorIndexed:
invlpg [rax]
db "glScissorIndexed", 0
ret

global glScissorIndexedv:function
align 16
glScissorIndexedv:
invlpg [rax]
db "glScissorIndexedv", 0
ret

global glViewportArrayv:function
align 16
glViewportArrayv:
invlpg [rax]
db "glViewportArrayv", 0
ret

global glViewportIndexedf:function
align 16
glViewportIndexedf:
invlpg [rax]
db "glViewportIndexedf", 0
ret

global glViewportIndexedfv:function
align 16
glViewportIndexedfv:
invlpg [rax]
db "glViewportIndexedfv", 0
ret

global glWindowPos2dARB:function
align 16
glWindowPos2dARB:
invlpg [rax]
db "glWindowPos2dARB", 0
ret

global glWindowPos2dvARB:function
align 16
glWindowPos2dvARB:
invlpg [rax]
db "glWindowPos2dvARB", 0
ret

global glWindowPos2fARB:function
align 16
glWindowPos2fARB:
invlpg [rax]
db "glWindowPos2fARB", 0
ret

global glWindowPos2fvARB:function
align 16
glWindowPos2fvARB:
invlpg [rax]
db "glWindowPos2fvARB", 0
ret

global glWindowPos2iARB:function
align 16
glWindowPos2iARB:
invlpg [rax]
db "glWindowPos2iARB", 0
ret

global glWindowPos2ivARB:function
align 16
glWindowPos2ivARB:
invlpg [rax]
db "glWindowPos2ivARB", 0
ret

global glWindowPos2sARB:function
align 16
glWindowPos2sARB:
invlpg [rax]
db "glWindowPos2sARB", 0
ret

global glWindowPos2svARB:function
align 16
glWindowPos2svARB:
invlpg [rax]
db "glWindowPos2svARB", 0
ret

global glWindowPos3dARB:function
align 16
glWindowPos3dARB:
invlpg [rax]
db "glWindowPos3dARB", 0
ret

global glWindowPos3dvARB:function
align 16
glWindowPos3dvARB:
invlpg [rax]
db "glWindowPos3dvARB", 0
ret

global glWindowPos3fARB:function
align 16
glWindowPos3fARB:
invlpg [rax]
db "glWindowPos3fARB", 0
ret

global glWindowPos3fvARB:function
align 16
glWindowPos3fvARB:
invlpg [rax]
db "glWindowPos3fvARB", 0
ret

global glWindowPos3iARB:function
align 16
glWindowPos3iARB:
invlpg [rax]
db "glWindowPos3iARB", 0
ret

global glWindowPos3ivARB:function
align 16
glWindowPos3ivARB:
invlpg [rax]
db "glWindowPos3ivARB", 0
ret

global glWindowPos3sARB:function
align 16
glWindowPos3sARB:
invlpg [rax]
db "glWindowPos3sARB", 0
ret

global glWindowPos3svARB:function
align 16
glWindowPos3svARB:
invlpg [rax]
db "glWindowPos3svARB", 0
ret
