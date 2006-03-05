function glStencilFuncSeparate( face, func, ref, mask )

% glStencilFuncSeparate  Interface to OpenGL function glStencilFuncSeparate
%
% usage:  glStencilFuncSeparate( face, func, ref, mask )
%
% C function:  void glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)

% 05-Mar-2006 -- created (generated automatically from header files)

if nargin~=4,
    error('invalid number of arguments');
end

moglcore( 'glStencilFuncSeparate', face, func, ref, mask );

return
