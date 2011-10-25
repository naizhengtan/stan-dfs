// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "debug.h"

extent_server::extent_server() {
  assert(pthread_mutex_init(&mutex,NULL)==0);
  //root dir with inum:0x000001
  std::string root("root\n");
  int tmp;//???
  put((unsigned long long)1,root,tmp);
}

//if id exist, will overwrite
int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
  ScopedLock mu(&mutex);
  
  extent_map_t::iterator it = extent_map.find(id);
  bool exist = it!=extent_map.end();
  //patch for set_attr
  if(*buf.begin()=='#'){//# stands for size
	if(exist){
	  char sign;int nsize;
	  std::istringstream is(buf);
	  is>>sign;
	  is>>nsize;
	  it->second.attr.size = nsize;
	  it->second.attr.ctime=it->second.attr.mtime = time(NULL);
	  //do the truncating or expanding thing
	  //get name
	  std::istringstream names(it->second.value);
	  std::string name;
	  names>>name;
	  //get others
	  int pos = it->second.value.find_first_of('\n');
	  std::string content  = it->second.value.substr(pos+1);
	  //put it back
	  std::ostringstream os;
	  os<<name<<"\n";
	  os<<content.substr(0,nsize);
	  for(int i=content.length();i<nsize;i++){
		os<<'\0';
	  }
	  it->second.value = os.str();
	  return extent_protocol::OK;
	}
	else{
	  return extent_protocol::NOENT;
	}
  }
  //new or modify a file
  unsigned int old_ctime = 0;
  if(exist){
	//?? put() means ctime,mtime,atime all Change??
   	//old_ctime = it->second.attr.ctime;
	extent_map.erase(id);
  }

  extent_data_t tmp;
  tmp.value = buf;
  if(old_ctime==0)
	tmp.attr.ctime=time(NULL);
  else
	tmp.attr.ctime=old_ctime;
  tmp.attr.atime=tmp.attr.mtime= time(NULL);
  //eliminate the name size
  tmp.attr.size=buf.size()-buf.find_first_of('\n')-1;
    
  extent_map.insert(extent_map_t::value_type(id,tmp));
  dprintf("extent_server: SUCESS put id:%d size:%d\n",id,tmp.attr.size);
  return extent_protocol::OK;
}


int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
  ScopedLock mu(&mutex);
  extent_map_t::iterator it = extent_map.find(id);
  if(it==extent_map.end())
	return extent_protocol::NOENT;//FIXME
  //find the id
  buf=it->second.value;
  it->second.attr.atime=time(NULL);
  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  ScopedLock mu(&mutex);
  extent_map_t::iterator it = extent_map.find(id);
  if(it==extent_map.end())
	return extent_protocol::NOENT;//FIXME
  //find the id
  a.size = it->second.attr.size;
  a.atime = it->second.attr.atime;
  a.mtime = it->second.attr.mtime;
  a.ctime = it->second.attr.ctime;
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  ScopedLock mu(&mutex);
  extent_map_t::iterator it = extent_map.find(id);
  if(it==extent_map.end()){
	//FIXME
  }
  else{
	extent_map.erase(it);
  }
  return extent_protocol::OK;
  //  return extent_protocol::IOERR;
}

