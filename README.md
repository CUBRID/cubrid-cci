## CUBRID Database Management System
CUBRID is a DBMS being supported by an active community of open source developers 
and provides better performance and features necessary for Web services. 

This Software is released under Apache License 2.0 and BSD according to CUBRID components.
For brevity, CUBRID Server Engine is under Apache License 2.0 and CUBRID APIs and Connectors are under BSD License.
For details, please refer to the CUBRID License Page(http://www.cubrid.org/cubrid).

Below You will see the brief list of sections to guide You to easily get started. 

## MAJOR REFERENCES
- CUBRID Official Site: http://www.cubrid.org ,  http://www.cubrid.com
- CUBRID Development Site(Global): http://jira.cubrid.org
- CUBRID Manuals: http://www.cubrid.org/manuals 
- CUBRID CCI Manuals (V11.0) : https://www.cubrid.org/manual/en/11.0/api/cci.html (Eng)
  https://www.cubrid.org/manual/ko/11.0/api/cci.html (Kor)

## DOWNLOADS and FILE REPOSITORIES
- http://www.cubrid.org/downloads
- http://ftp.cubrid.org
## HOW TO BUILD CUBRID CCI Driver
### CUBRID CCI Driver Source Build Guide
  * Requirement
    - GNU Developer Toolset 8 or higher (Linux)
    - Git (Linux, Windows) 1.7.6 or higher
    - MSVC 2017 or 2017 or 2012 (Windows, 2017 is recommended)
    - CMake (Linux, Windows)

* Windows
  ```
  build)
  win\build.bat 
  
  help)
  win\build.bat /h
  ```
  - IMPORTANT: Add (git-installation-directory)/usr/bin to %PATH% environment variable
    
* Linux

  ```
  build)
  ./build.sh
  
  help)
  ./build.sh --help
  ```

GETTING HELP
============
If You encounter any difficulties with getting started, or just have some
questions, or find bugs, or have some suggestions, we kindly ask You to 
post your thoughts on CUBRID Forum at https://www.reddit.com/r/CUBRID/.

Sincerely,
Your CUBRID Development Team.
