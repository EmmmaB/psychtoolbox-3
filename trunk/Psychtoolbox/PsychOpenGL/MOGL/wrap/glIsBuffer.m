function r = glIsBuffer( buffer )

% glIsBuffer  Interface to OpenGL function glIsBuffer
%
% usage:  r = glIsBuffer( buffer )
%
% C function:  GLboolean glIsBuffer(GLuint buffer)

% 05-Mar-2006 -- created (generated automatically from header files)

if nargin~=1,
    error('invalid number of arguments');
end

r = moglcore( 'glIsBuffer', buffer );

return
