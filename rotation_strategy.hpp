#ifndef __ROTATION_STRATEGY_HPP__
#define __ROTATION_STRATEGY_HPP__

#include "aixlog.hpp"
#include <iomanip>
#include <cstring>

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fnmatch.h>

class Filez
{
  public:
    enum List_Types {
      LT_Files = 0x01,
      LT_Dirs = 0x02,
      LT_Hidden = 0x04,
      LT_Normal = LT_Files | LT_Dirs,
      LT_All = LT_Files | LT_Dirs | LT_Hidden
    };

    enum File_Types {
      FT_None,
      FT_Regular,
      FT_Directory,
      FT_ChrDev,
      FT_BlkDev,
      FT_Fifo,
      FT_Link,
      FT_Socket
    };

    typedef std::vector<std::string> StrVec;

    static StrVec directory_list(const std::string& path,
                                 uint8_t Filter=Filez::LT_Normal,
                                 const std::string& mask="" )
    {
      StrVec ret;

      if(dexists(path))
      {
        DIR *dir;
        struct dirent *ent;

        if ((dir = opendir(path.c_str())) != NULL)
        {
          /* print all the files and directories within directory */
          while ((ent = readdir(dir)) != NULL)
          {
            if(ent->d_name[0] == '.' && !(Filter & LT_Hidden))
              continue;

            if(!mask.empty() && (fnmatch(mask.c_str(), ent->d_name,0)!=0))
              continue;

            if(ent->d_type == DT_DIR && Filter & LT_Dirs)
              ret.push_back(ent->d_name);
            else if(ent->d_type != DT_DIR && Filter & LT_Files)
              ret.push_back(ent->d_name);
          }
          closedir (dir);
        }
      }
      return ret;
    }

    static void remove(const std::string& path, const std::string& mask)
    {
      StrVec list;

      list=directory_list(path, Filez::LT_Files, mask);

      for(const auto& file:list)
        std::remove(file.c_str());
    }

    static bool exists(const std::string& name, struct stat *pStat=0)
    {
      struct stat st;

      if (!pStat)
        pStat=&st;

      std::memset(pStat,0,sizeof(struct stat));

      return stat(name.c_str(), pStat) == 0;
    }

    static bool fexists(const std::string& name, struct stat *pStat=0)
    {
      struct stat st;

      if (!pStat)
        pStat=&st;

      return exists(name, pStat) && (pStat->st_mode & S_IFREG);
    }

    static bool dexists(const std::string& name, struct stat *pStat=0)
    {
      struct stat st;

      if (!pStat)
        pStat=&st;

      return exists(name, pStat) && (pStat->st_mode & S_IFDIR);
    }

    static bool remove(const std::string& name)
    {
      return std::remove(name.c_str()) == 0;
    }

    static uint32_t getFileType(const std::string& name)
    {
      struct stat st;

      if(exists(name, &st))
      {
        if(S_ISREG(st.st_mode))  return FT_Regular;
        if(S_ISDIR(st.st_mode))  return FT_Directory;
        if(S_ISCHR(st.st_mode))  return FT_ChrDev;
        if(S_ISBLK(st.st_mode))  return FT_BlkDev;
        if(S_ISFIFO(st.st_mode)) return FT_Fifo;
        if(S_ISLNK(st.st_mode))  return FT_Link;
        if(S_ISSOCK(st.st_mode)) return FT_Socket;
      }

      return FT_None;
    }

    static std::string dirname_r(const std::string& path)
    {
      if (path.empty() || path.find_first_of('/') == std::string::npos)
        return ".";

      std::string ret=path;

      while(!ret.empty() && ret.back()=='/')
        ret.pop_back();

      if(ret.empty())
        return ".";

      ret.erase(ret.find_last_of('/'));

      return ret;
    }

    static std::string basename_r(const std::string& path)
    {
      if(path.empty())
        return ".";

      if(path.find_first_not_of('/') == std::string::npos)
        return "/";

      std::string ret=path;

      while(!ret.empty() && ret.back()=='/')
        ret.pop_back();

      if(ret.empty())
        return ".";

      return ret.substr(ret.find_last_of('/')+1);
    }
};


struct rotation_strategy : public AixLog::SinkFile::Strategy
{
  struct rotation_info {
    std::string main;
    size_t current_log_size;
    std::vector<std::string> rotation_file_list;
    uint32_t last_count;
    uint32_t first_count;
    bool contiguous;

    rotation_info(){reset();}
    void reset()
    {
      main.erase();
      current_log_size=0;
      rotation_file_list.clear();
      last_count=0;
      first_count=0;
      contiguous=false;
    }
  };

  // minimum number of digits to use for the rotation count
  static const uint8_t rotate_field_width = 2;

  // if rotation count is 0, disable rotation.
  // otherwise rotate if the file size is > the rotate size
  rotation_strategy(AixLog::Severity s,
                    AixLog::Type t,
                    const std::string& fn,
                    const std::string& fmt = "%Y%m%d %H:%M:%S.#ms #file(#line) [#severity]: #message",
                    bool apnd = true,
                    uint32_t rot_count = 0,
                    uint32_t rot_size = 0)
    : AixLog::SinkFile::Strategy(s, t, fn, fmt), append(apnd), rotation_count(rot_count), rotation_size(rot_size)
  { }

  virtual void open(std::ofstream& ofs)const
  {
    std::ios_base::openmode mode = std::ios_base::out;

    if (append)
      mode |= std::ios_base::app;
    else
      mode |= std::ios_base::trunc;

    rotate();

    ofs.open(filename.c_str(), mode);
  }

  void get_rotation_info(const std::string& name, rotation_info& info)const
  {
    std::string num="[0-9]";
    std::string wild=name+".";
    std::string dir,base;
    struct stat st;

    dir=Filez::dirname_r(name);
    base=Filez::basename_r(name);

    for(uint32_t cnt=0; cnt < rotate_field_width ; cnt++)
      wild+=num;

    info.reset();

    if(Filez::fexists(name, &st))
    {
      info.main=name;
      info.current_log_size=st.st_size;
    }

    info.rotation_file_list=Filez::directory_list(dir, Filez::LT_Files, wild);

    std::sort(info.rotation_file_list.begin(), info.rotation_file_list.end());

    if(!info.rotation_file_list.empty())
    {
      std::string str;

      // get number of last rotate
      str=info.rotation_file_list.back();

      str=str.substr(str.length()-rotate_field_width);
      info.last_count=std::stoi(str);

      // get number of first rotate
      str=info.rotation_file_list.front();
      str=str.substr(str.length()-rotate_field_width);
      info.first_count=std::stoi(str);

      // if (last - first + 1 == list.size()), we are contiguous
      info.contiguous = ((info.last_count - info.first_count + 1) == info.rotation_file_list.size());
    }
  }

  void rotate()const
  {
    if(rotation_count == 0)
      return;

    rotation_info info;

    get_rotation_info(filename, info);

    if(info.main.empty() ||
       (rotation_size > 0 && info.current_log_size < rotation_size) )
      return;

    // This covers the rotation_size == 0 scenareo, in that case, this will
    // also be true
    if(info.current_log_size >= rotation_size)
    {
      // we need to rotate
      std::function<void(uint32_t,uint32_t)> do_rotation;
      std::function<std::string(const std::string&, uint32_t)> make_fn;

      make_fn=[](const std::string& base, uint32_t idx) -> std::string {
        std::ostringstream os;
        os << std::setfill('0') << std::setw(rotate_field_width) << idx;
        return base + "." + os.str();
      };

      do_rotation=[&](uint32_t f, uint32_t s){
        std::string fn, sn;

        fn=make_fn(info.main, f);
        sn=make_fn(info.main, s);

        //check if the first file is there
        if(std::find(info.rotation_file_list.begin(),
                     info.rotation_file_list.end(),
                     fn) == info.rotation_file_list.end())
        {
          // file is missing
          return;
        }

        if(std::find(info.rotation_file_list.begin(),
                     info.rotation_file_list.end(),
                     sn) != info.rotation_file_list.end())
        {
          //found it
          do_rotation(s, s+1);
        }
        std::rename(fn.c_str(),sn.c_str());

      }; //end do_rotation

      do_rotation(1,2);
      std::string newfn=make_fn(info.main, 1);

      std::rename(info.main.c_str(), newfn.c_str());

      get_rotation_info(filename, info);

      // check if we have too many
      while(info.rotation_file_list.size() > rotation_count)
      {
        auto fileiter=info.rotation_file_list.end()-1;
        std::remove(fileiter->c_str());
        info.rotation_file_list.erase(fileiter);
      }
    }
  }


  bool append;
  uint32_t rotation_count;
  uint32_t rotation_size;
};

#endif //__ROTATION_STRATEGY_HPP__
