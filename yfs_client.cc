// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);

}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;

  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

 release:

  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

 release:
  return r;
}


int
yfs_client::create(inum parent,const char* name,inum &inum){
  int r =OK;
  //check if the file is exist
  int inode;
  std::string content,fname;
  std::ostringstream os;

  int re = ec->get(parent,content);//??content
  if(re != extent_protocol::OK){
	return re;
  }
  std::istringstream is(content);
  is>>fname;//eliminate dir's name
  do{
	is>>fname;//get the file name
	is>>inode;//get the inum of the file
	if(fname == name){//there is already have the file name
	  r = EXIST;
	  goto release;
	}
  }while(!is.eof());

  //generate the file's inum
  //FIXME
  srandom(time(NULL));
  inum = random();
  inum |= 0x80000000;
  printf("create %016llx [%s]\n",inum,name);

  //new (name,inum), add node in extent_server
  re = ec->put(inum,name);
  if(re == extent_protocol::NOENT){//the inum is collision,FIXME
	r = EXIST;
	goto release;
  }
  else if(re != extent_protocol::OK){//Other error
	r = IOERR;
	goto release;
  }
  printf("add [%s] to parent %016llx\n",name,parent);

  //add the file to the dir
  os<<content;
  os<<name<<"\n";
  os<<inum<<"\n";
  re = ec->put(parent,content);
  if(re!=extent_protocol::OK)
	r = IOERR;

 release:
  return r;
  
}

int 
yfs_client::readdir(inum parent,std::map<std::string,inum> &map){
  std::string content;
  int re = ec->get(parent,content);
  if(re!=extent_protocol::OK){
	return IOERR;
  }
  //parse the string
  inum inode;
  std::string fname;
  std::istringstream is(content);
  is>>fname;//eliminate the dir name
  do{
	is>>fname;
	is>>inode;
	if(fname!="")
	  map.insert(std::pair<std::string,inum>(fname,inode));
  }while(!is.eof());
  
  return OK;
}

int yfs_client::lookup(inum parent,const char* name,inum& finum){
  std::map<std::string,inum> map;
  finum = -1;
  //get the content of the dir
  int re = readdir(parent,map);
  if(re!=OK)
	return re;
  //check if there is name
  std::map<std::string,inum>::iterator it = map.find(name);
  if(it!=map.end())
	finum = it->second;
  return OK;
}
