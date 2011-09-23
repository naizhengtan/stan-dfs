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
  if(exist)
	extent_map.erase(id);
  //check wether this node is exist
  //---if(!exist){//the file/dir is new
  extent_data_t tmp;
  tmp.value = buf;
  tmp.attr.atime=tmp.attr.mtime=tmp.attr.ctime = time(NULL);//FIXME
  tmp.attr.size=buf.size();
  
  extent_map.insert(extent_map_t::value_type(id,tmp));
  dprintf("extent_server: SUCESS put id:%d value:%s\n",id,buf);
  return extent_protocol::OK;
  //---}
  /*---
	else{//there is a exist file/dir
	return extent_protocol::NOENT;//FIXME
	}
  */
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

