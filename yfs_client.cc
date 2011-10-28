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
  lc = new lock_client(lock_dst);
  //lc = new lock_client_cache(lock_dst);
  srandom(getpid());
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

  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("//yfs// getfile %016llx -> sz %llu\n", inum, fin.size);

 release:
  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the directory lock

  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

  printf("//yfs// getdir %016llx\n", inum);
 release:
  return r;
}


int
yfs_client::create(inum parent,const char* name,inum &inum){
  int ret = createHelper(parent,name,inum,0);
  return ret;
}

int
  yfs_client::createHelper(inum parent,const char* name,inum &inum,int type){
  //lock the parent dir
  lock_client::rScopedLock dirlock(lc,parent);

  int r =OK;
  //check if the file is exist
  int inode=-1;
  std::string content,fname,dirname;
  std::ostringstream os;
  std::string fullName(name);

  int re = ec->get(parent,content);//??content
  if(re != extent_protocol::OK){
	return re;
  }
  std::istringstream is(content);
  is>>dirname;//eliminate dir's name
  printf("==current dir==\n%s \n====\n",content.c_str());
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
  inum = random();
  if(type == 0)
	inum |= 0x80000000;
  else if(type==1)
	inum &= 0x7FFFFFFF;
  printf("//yfs// create file %016llx [%s]\n",inum,name);

  //new (name,inum), add node in extent_server
  fullName+='\n';
  //re = ec->put(inum,name);
  re = ec -> put(inum,fullName.c_str());
  if(re == extent_protocol::NOENT){//the inum is collision,FIXME
	r = EXIST;
	goto release;
  }
  else if(re != extent_protocol::OK){//Other error
	r = IOERR;
	goto release;
  }
  //lock the new created file
  //lock_client::ScopedLock filelock(lc,inum);

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
  //lock file
  lock_client::rScopedLock filelock(lc,finum);

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
  //lock file
  lock_client::rScopedLock filelock(lc,finum);

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
  printf("content:====\n%s\n=====\n",buf);
  return OK;
}

//very similar to the file creation
//FIXME
//replica of function:create
int yfs_client::mkdir(inum parent, const char* name,inum &dinum){
  
  int ret = createHelper(parent,name,dinum,1);
  return ret;

}

int yfs_client::unlink(inum parent,const char* name ){
  //lock dir
  lock_client::rScopedLock dirlock(lc,parent);

  std::string content;
  int ret = ec->get(parent,content);
  if(ret!=extent_protocol::OK)
	return ret;
  inum fnode=-1,loop_inode=-1;
  int exist=0;
  std::string loop_fname,dirname;
  std::istringstream is(content);
  std::ostringstream os;

  is>>dirname;//eliminate the dir name
  os<<dirname<<"\n";
  printf("!!!%s\n",dirname.c_str());
  do{
	is>>loop_fname;//file name
	is>>loop_inode;//file node
	printf("====%s %s %d %d====\n",loop_fname.c_str(),name,loop_inode,(loop_fname == name));
	if(loop_inode == -1)
	  break;
	if(loop_fname == name){
	  fnode = loop_inode;
	  exist=1;
	  loop_inode=-1;
	  continue;
	}
	os<<loop_fname<<"\n";
	os<<loop_inode<<"\n";
	loop_inode = -1;
  }while(!is.eof());

  //replace the parent's content
  if(exist==0)
	return NOENT;

  //remove the file/dir from its parent dir
  ret = ec->put(parent,os.str());
  if(ret!=extent_protocol::OK)
	return checkErrorCode(ret);

  //lock file
  lock_client::rScopedLock filelock(lc,fnode);

  //remove it from extent server
  ret = ec->remove(fnode);
  if(ret!=extent_protocol::OK)
	return checkErrorCode(ret);
  printf("//yfs// remove the [%s] from parent:%016llx\n",name,parent);
  return OK;
}

int yfs_client::checkErrorCode(int err){
  if(err == extent_protocol::RPCERR){
	printf("===CHECK ERROR CODE=== RPCERR\n");
	return RPCERR;
  }else if(err == extent_protocol::NOENT){
	printf("===CHECK ERROR CODE=== NOENT\n");
	return NOENT;
  }else if(err == extent_protocol::IOERR){
	printf("===CHECK ERROR CODE=== IOERR\n");
	return IOERR;
  }
}
