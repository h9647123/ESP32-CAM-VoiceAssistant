#pragma once
#include <string>
#include <vector>
#include <algorithm>
// Narrow displays use labelled rows instead of tiny, clipped multi-column grids.
// Keep code fences literal and process only a header followed by a real divider.
static std::string camMarkdownTables(const std::string& input) {
  if(input.find('|')==std::string::npos)return input;
  auto trim=[](std::string s){const auto a=s.find_first_not_of(" \t\r");if(a==s.npos)return std::string();return s.substr(a,s.find_last_not_of(" \t\r")-a+1);};
  auto cells=[&](const std::string& row){std::vector<std::string> values;std::string cell;bool code=false;
    for(size_t i=0;i<row.size();++i){char c=row[i];if(c=='\\' && i+1<row.size() && row[i+1]=='|'){cell+='|';++i;continue;}if(c=='`')code=!code;
      if(c=='|' && !code){values.push_back(trim(cell));cell.clear();}else cell+=c;}
    values.push_back(trim(cell));if(!values.empty()&&values.front().empty())values.erase(values.begin());if(!values.empty()&&values.back().empty())values.pop_back();return values;};
  std::vector<std::string> lines;size_t at=0;
  while(at<input.size()){const auto end=input.find('\n',at);lines.push_back(input.substr(at,end==input.npos?end:end-at));if(end==input.npos)break;at=end+1;}
  std::string out;bool fenced=false;
  for(size_t i=0;i<lines.size();++i){const auto line=trim(lines[i]);if(line.compare(0,3,"```")==0 || line.compare(0,3,"~~~")==0)fenced=!fenced;
    bool table=!fenced && i+1<lines.size() && lines[i].find('|')!=std::string::npos;
    auto header=cells(lines[i]);std::vector<std::string> divider;
    if(table){divider=cells(lines[i+1]);table=header.size()>=2&&divider.size()==header.size();
      for(const auto& c:divider){unsigned dashes=0;for(char x:c){if(x=='-')++dashes;else if(x!=':'&&x!=' '&&x!='\t')table=false;}table=table&&dashes>=3;}}
    if(!table){out+=lines[i];if(i+1<lines.size()||(!input.empty()&&input.back()=='\n'))out+='\n';continue;}
    i+=2;
    for(;i<lines.size()&&lines[i].find('|')!=std::string::npos;++i){const auto row=cells(lines[i]);
      for(size_t c=0;c<std::max(header.size(),row.size());++c){if(c)out+='\n';if(c<header.size())out+=header[c]+": ";if(c<row.size())out+=row[c];}
      out+="\n\n";}
    --i;
  }
  return out;
}
