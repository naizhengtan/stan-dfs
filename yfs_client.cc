// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client.h"
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
  // You modify this function for Lab 3
  // - hold and release the file lock

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
  // You modify this function for Lab 3
  // - hold and release the directory lock

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
  int inode=-1;
  std::string content,fname,dirname;
  std::ostringstream os;
  int re = ec->get(parent,content);//??content
  if(re != extent_protocol::OK){
	return re;
  }
  std::istringstream is(content);
  is>>dirname;//eliminate dir's name
  printf("!!\n%s %s \n!!",dirname.c_str(),content.c_str());
  do{
	is>>fname;//get the file name
	is>>inode;//get the inum of the file
	if(fname == name){//there is already have the file name
	  r = EXIST;
	  goto release;
	}
	if(inode==-1)//???here has a infinitive loop???
	  break;
	inode=-1;
	//printf("!!%s %s %016llx\n",fname.c_str(),name,inode);
  }while(!is.eof());

  //generate the file's inum
  //FIXME
  //srandom(time(NULL));
  inum = random();
  inum |= 0x80000000;
  printf("//yfs// create file %016llx [%s]\n",inum,name);

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

  //add the file to the dir
  os<<content;
  os<<name<<"\n";//FIXME, no space(" ") permited in the name
  os<<inum<<"\n";
  re = ec->put(parent,os.str());
  if(re!=extent_protocol::OK)
	r = IOERR;

  printf("//yfs// add [%s] to parent %016llx\n",name,parent);

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
  inum inode=-1;
  std::string fname,dirname;
  std::istringstream is(content);
  is>>dirname;//eliminate the dir name
  printf("//yfs// readdir parent:%016llx[%s]\n",parent,dirname.c_str());
  do{
	is>>fname;
	is>>inode;
	if(inode!=-1){
	  map.insert(std::pair<std::string,inum>(fname.c_str(),inode));
	  //printf("      map insert (%s,%016llx)\n",fname.c_str(),inode);
	}
	else
	  break;
	inode=-1;
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
  std::string searchname(name);
  std::map<std::string,inum>::iterator it = map.find(searchname);
  if(it!=map.end())
	finum = it->second;
  else{
	/**debug
    for(it=map.begin();it!=map.end();it++){
	  printf("       [%s] %016llx\n",it->first.c_str(),it->second);
	}
	**/
  }
  printf("//yfs// lookup from parent:%016llx [%s] finum:%016llx\n",parent,name,finum);
  return OK;
}

int
yfs_client::setattr_size(inum finum,unsigned long long size){
  std::string message = "#";
  message += filename(size);
  int re =  ec->put(finum,message);
  if(re!=extent_protocol::OK){
	return NOENT;
  }
  printf("//yfs:setattr_size// set size of %016llx to size %lld\n",finum,size);
  return OK;
}

int 
yfs_client::read(inum finum,unsigned long long size,
				 unsigned long long off, char* buf){
  std::string content;
  int re = ec->get(finum,content);
  if(re!=extent_protocol::OK){
	return NOENT;
  }
  //not contain the file name
  int pos = content.find_first_of('\n');
  content = content.substr(pos+1);
  //read the content
  content = content.substr(off,size);
  if(size>content.length())
	size = content.length();
  for(int i=0;i<size;i++){
	buf[i] = content[i];
  }
  printf("//yfs:read// read file %016llx size:%d off:%d\n",finum,size,off);
  printf("read content:====\n%s\n====\n",buf);
  return OK;
}

/**
   example of this:
   origin: 0123456789
   off =5 size=3 buf=abc
   result: 01234abc89
 */
int
yfs_client::write(inum finum,unsigned long long size, 
				  unsigned long long off, const char* buf){
  std::string content;
  int re = ec->get(finum,content);
  if(re!=extent_protocol::OK){
	return NOENT;
  }
  //not contain the file name
  std::string name;
  std::istringstream is(content);
  is>>name;

  int pos = content.find_first_of('\n');
  content = content.substr(pos+1);
  //modify the content of file
  std::ostringstream os;
  os<<name<<"\n";
  os<<content.substr(0,off);
  //add the hole to the file with '\0'
  for(int i=content.length();i<off;i++)
	os<<'\0';
  //add the buf
  for(int i=0;i<size;i++)
	os<<buf[i];
  int left = content.length()-off-size;
  if(left>0)
	os<<content.substr(off+size);
  //put it back
  re = ec->put(finum,os.str());
  if(re!=extent_protocol::OK){
	return IOERR;
  }
  printf("//yfs// write to file%016llx size:%d off:%d\n",finum,size,off);
  printf("content:====\n%s\n=====",buf);
  return OK;
}
