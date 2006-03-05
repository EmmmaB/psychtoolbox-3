function buffers = glGenBuffers( n )

% glGenBuffers  Interface to OpenGL function glGenBuffers
%
% usage:  buffers = glGenBuffers( n )
%
% C function:  void glGenBuffers(GLsizei n, GLuint* buffers)

% 05-Mar-2006 -- created (generated automatically from header files)

% ---allocate---

if nargin~=1,
    error('invalid number of arguments');
end

buffers = uint32(0);

moglcore( 'glGenBuffers', n, buffers );

return
