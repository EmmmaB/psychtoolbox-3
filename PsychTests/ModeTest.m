% ModeTest%% Test Screen('PutImage') modes.% 1/13/99  dgp  Wrote it.% 10/7/99  dgp  CloseAll when done.clear screen;s=127*sin(0:.1:10);n=length(s);s=s'*ones(1,n);w=Screen(0,'OpenWindow',128,[],8);s=round(s)+256;right=[n,0,n,0];down=[0,n,0,n];r=[0,0,n,n];Screen(w,'PutImage',s+128,r,'srcCopy');r=r+down;Screen(w,'PutImage',s+128,r,'srcCopyQuickly');r=r+down;Screen(w,'PutImage',s,r,'addOverQuickly');r=r+down;Screen(w,'PutImage',s,r,'addOverParallelQuickly');r=r+right;Screen(w,'PutImage',s+128+512,r,'srcCopy');waitsecs(2)screen('closeall');