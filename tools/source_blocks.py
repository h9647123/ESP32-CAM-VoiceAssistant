import re

def endblock(s,start):
    depth=0;i=start
    while i<len(s):
        if s.startswith('//',i):
            j=s.find('\n',i);i=len(s) if j<0 else j;continue
        if s.startswith('/*',i):
            j=s.find('*/',i+2);assert j>=0;i=j+2;continue
        if s.startswith('R"',i):
            j=s.find('(',i+2);tag=s[i+2:j];k=s.find(')'+tag+'"',j+1);assert k>=0;i=k+len(tag)+2;continue
        if s[i] in ('"',"'"):
            q=s[i];i+=1
            while i<len(s):
                if s[i]=='\\':i+=2;continue
                if s[i]==q:i+=1;break
                i+=1
            continue
        if s[i]=='{':depth+=1
        elif s[i]=='}':
            depth-=1
            if depth==0:return i+1
        i+=1
    raise ValueError('unbalanced block')

def locate(s,name):
    pat=re.compile(r'^(?:static\s+)?[\w:<>&*]+(?:\s+[\w:<>&*]+)*\s+'+re.escape(name)+r'\([^;]*?\)\s*\{',re.M)
    ms=list(pat.finditer(s));assert len(ms)==1,(name,len(ms))
    m=ms[0];return m.start(),endblock(s,m.end()-1)

def get(s,name):
    a,b=locate(s,name);return s[a:b]

def put(s,name,new):
    a,b=locate(s,name);return s[:a]+new+s[b:]

