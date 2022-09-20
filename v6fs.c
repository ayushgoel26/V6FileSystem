/* Author: Ayush Goel , Reva Gupta 
* UTD ID: AXG190164, RXG190061
* CS 5348.001 Operating Systems
* Prof. S Venkatesan
* Project - 2 Deliverable - 2
************
* Compilation: $ cc -o fsgen v6fs.c -std=c99
* Run using :  $ ./fsgen
* Use h command to get the help manual for the program
******************/

#include<stdio.h> 
#include<stdbool.h>  
#include<fcntl.h>   
#include<unistd.h> 
#include<errno.h> 
#include<string.h>
#include<stdlib.h>
#include<time.h>
#include<ctype.h>

#define FREE_SIZE 151                                                                                             // size of free array
#define I_SIZE 200                                                                                                // size of inode array
#define BLOCK_SIZE 1024                                                                                           // size of one Data block
#define ADDR_SIZE 11                                                                                              // size of address array in the inode array
#define INPUT_SIZE 256                                                                                            // max size string that user inputs 

typedef struct {
  unsigned short isize;
  unsigned short fsize;
  unsigned short nfree;
  unsigned int free[FREE_SIZE];
  unsigned short ninode;
  unsigned short inode[I_SIZE];
  char flock;
  char ilock;
  unsigned short fmod;
  unsigned short time[2];
} superblock_type;
superblock_type superBlock;

typedef struct {
  unsigned short flags;
  unsigned short nlinks;
  unsigned short uid;
  unsigned short gid;
  unsigned int size;
  unsigned int addr[ADDR_SIZE];
  unsigned short actime[2];
  unsigned short modtime[2];
} inode_type;
inode_type inode, curr_inode;

typedef struct {
  unsigned short inode;
  unsigned char filename[14];
} dir_type;
dir_type curr_dir;

typedef struct {
  int sliceIndex, addrIndex, inodeNumber;
  bool found;
} emptySlice;
emptySlice slice;

typedef struct {
  char* name;
  char* path;
} file_details;
file_details file;

int fileDescriptor ;		                                                                                          // file descriptor
bool fs_initialised = false;                                                                                      // flag if filesystem is initialised or not
const unsigned short inode_alloc_flag = 0100000;                                                                  // flag for if inode is allocated
const unsigned short dir_flag = 040000;                                                                           // flag for if the inode is for directory
const unsigned short dir_large_file = 010000;                                                                     // flag for if the directory/file is large
const unsigned short dir_access_rights = 000777;                                                                  // User, Group, & World have all access privileges
const unsigned short INODE_SIZE = 64;                                                                             // inode size

int preInitialization();                                                                                          // pre-initialization of file system
int initfs(char* path, unsigned short total_blcks,unsigned short total_inodes);                                   // initialises filesystem
void add_block_to_free_list( int blocknumber , unsigned int *empty_buffer );                                      // adds block to free list
int get_block_from_free_list();                                                                                   // gets available block number from free list
void create_root();                                                                                               // creating root of the filesystem         
void cpin();                                                                                                      // copy file in to filesystem 
void cpout(char* fileDest);                                                                                       // copy file out of filesystem 
void mkdir();                                                                                                     // crearting new directory
bool cd(char *directory_path);                                                                                    // changing working directory
void rm();                                                                                                        // removing a file or directory
void ls(char* directory_path);                                                                                    // printing the current directoiries content
void searchSlice();                                                                                               // searching for empty spot in a directory 
bool isDirectory(unsigned short n);                                                                               // check if the passed path is for a directory or a file
void splitpath();                                                                                                 // splitting the path into basic path and the file/directory name
bool isEmptyDirectory(inode_type inode);                                                                          // check if directory is empty                 
bool validateName();                                                                                              // validate if file or folder name is not longer than 14 characters

int main() {
  char input[INPUT_SIZE];
  char* splitter;
  printf("\033[1;36mEnter h to access help manual.\033[0m\n");
 
  printf("\033[1mSize of super block = %lu , size of i-node = %lu\033[0m\n", sizeof(superBlock), sizeof(inode));
  printf("\033[1;32mEnter command:\033[0m\n");
  while (1) {
    printf(">> ");
    scanf(" %[^\n]s", input);
    splitter = strtok(input, " ");
    if (strcmp(splitter, "initfs") == 0) {
      preInitialization();
      splitter = NULL;
    } else if (strcmp(splitter, "cpin") == 0) {
      char* fileSource = strtok(NULL, " ");                                                                       // reading the source file to copy inside the file system
      file.path = strtok(NULL, " ");                                                                              // reading name of the destination file
      if (!fs_initialised) printf("\033[1;31mFile system not initialised. Use initfs to initialise.\033[0m\n");
      else {
        if (fileSource == NULL || file.path == NULL)                                                              // if parameters are missing then raise an error
          printf("\033[1;31mMissing parameters for cpin. Check help(h) for more info.\033[0m\n"); 
        else cpin(fileSource);                                                                                    // if all good then call function
      }
      splitter = NULL;
    } else if (strcmp(splitter, "cpout") == 0) {
      file.path = strtok(NULL, " ");
      char* fileDest = strtok(NULL, " ");
      if (!fs_initialised) printf("\033[1;31mFile system not initialised. Use initfs to initialise.\033[0m\n");   // if file system is not initialised then error
      else {
        if (fileDest == NULL || file.path == NULL)                                                                // if parameters are missing then raise an error
          printf("\033[1;31mMissing parameters for cpout. Check help(h) for more info.\033[0m\n"); 
        else cpout(fileDest);                                                                                     // if all good then call function
      }
      splitter = NULL;
    } else if (strcmp(splitter, "mkdir") == 0){
      file.path = strtok(NULL, " ");                                                                              // reading name of new directory. 
      if (!fs_initialised) printf("\033[1;31mFile system not initialised. Use initfs to initialise.\033[0m\n");   // if file system is not initialised then error
      else {
        if (file.path == NULL)                                                                                    // if no directory given then error
          printf("\033[1;31mDirectory name not specified. Check help(h) for more info.\033[0m\n");                
        else mkdir();                                                                                             // if all good then call function
      } 
    } else if (strcmp(splitter, "cd") == 0){  
      char *directory_path = strtok(NULL, " ");                                                                   // reading name for new directory. 
      if (!fs_initialised) printf("\033[1;31mFile system not initialised. Use initfs to initialise.\033[0m\n");   // if file system is not initialised then error
      else {
        if (directory_path == NULL)                                                                               // if no path given then error
          printf("\033[1;31mPath not specified. Check help(h) for more info.\033[0m\n");                
        else cd(directory_path);                                                                                  // if all good then call function
    }} else if (strcmp(splitter, "rm") == 0){
      file.path = strtok(NULL, " ");                                                                              // reading name for item to remove. 
      if (!fs_initialised) printf("\033[1;31mFile system not initialised. Use initfs to initialise.\033[0m\n");   // if file system is not initialised then error
      else {
        if (file.path == NULL)                                                                                    // if no directory given then error
          printf("\033[1;31mNo name specified. Check help(h) for more info.\033[0m\n");                
        else rm();                                                                                                // if all good then call function
      }    
    } else if (strcmp(splitter, "h") == 0) {                                                                      // help text 
      printf("\033[1;36minitfs : \033[0mInitilizes the file system and redesigning the Unix file system to accept large\n"\
            "         files of up tp 4GB, expands the free array to 152 elements, expands the i-node array to\n"\
            "         200 elemnts, doubles the i-node size to 64 bytes and other new features as well.\n"\
            "         \033[1;32m>> initfs (file path) (# of total system blocks) (# of System i-nodes)\033[0m\n\n"\
            "\033[1;36mcpin   : \033[0mCopies a file in to the v6 file system \n"\
            "         \033[1;32m>> cpin (external v6 file path) (filename in v6 root path)\033[0m\n\n"\
            "\033[1;36mcpout  : \033[0mCopies a file out of the v6 file system\n"\
            "         \033[1;32m>> cpout (filename in v6 root path) (external v6 file path)\033[0m\n\n"\
            "\033[1;36mmkdir  : \033[0mmakes a new directory with entries . and .. \n"\
            "         \033[1;32m>> mkdir (directory path)\033[0m\n\n"\
            "\033[1;36mrm     : \033[0mremoves a file or an empty directory\n"\
            "         \033[1;32m>> rm (empty directory/file)\033[0m\n\n"\
            "\033[1;36mcd     : \033[0mchange working directory of the v6 file system to the directory specified\n"\
            "         \033[1;32m>> cd (directory path)\033[0m\n\n"\
            "\033[1;36mls     : \033[0mlists the contents of the current directory\n"\
            "         \033[1;32m>> ls\033[0m\n\n"\
            "\033[1;36mh      : \033[0mhelp text - prints all supported commands\n"\
            "         \033[1;32m>> h\033[0m\n\n"\
            "\033[1;36mq      : \033[0msave all work and exit the program.\n"\
            "         \033[1;32m>> q\033[0m\n\n"\
            "\033[0;31m***File name is limited to 14 characters.\033[0m\n");
    } else if(strcmp(splitter, "ls") == 0){
      if (!fs_initialised) printf("\033[1;31mFile system not initialised. Use initfs to initialise.\033[0m\n");   // if file system is not initialised then error
      else ls((char*) curr_dir.filename);                                                                         // listing the contents
    } else if (strcmp(splitter, "q") == 0) {
      lseek(fileDescriptor, BLOCK_SIZE, 0);
      write(fileDescriptor, &superBlock, BLOCK_SIZE);                                                             // saving the superblock for changes made so far
      printf("Quitting!\n");
      return 0;
    } else {
      if (!fs_initialised) printf("\033[1;31mFile system not initialised. Use initfs to initialise.\033[0m\n");   // if file system is not initialised then error
      else printf("\033[1;31mUnknown Command. Check help(h) for more info.\033[0m\n");                            // unknown command entered. 
    }
  }
}

// Sanity check before initialising fs
int preInitialization(){
  char *n1, *n2, *filepath;
  unsigned int numBlocks, numInodes;

  filepath = strtok(NULL, " ");                                                                                   // reading arguments of initfs
  n1 = strtok(NULL, " ");
  n2 = strtok(NULL, " ");

  if(access(filepath, F_OK) != -1) {
    fileDescriptor = open(filepath, O_RDWR, 0700);
    if(fileDescriptor == -1){
        printf("\033[1;31mFilesystem already exists. open() failed with error [%s]\033[0m\n", strerror(errno));
        return 1;
    }
    printf("\033[1;32mFilesystem already exists and the same will be used.\033[0m\n");

    // Loading information from the existing file system  (not provided as starter code)
    lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET); 
    read(fileDescriptor, &superBlock, BLOCK_SIZE);                                                                // reading superblock
    lseek(fileDescriptor, 2 * BLOCK_SIZE, 0); 
    read(fileDescriptor, &curr_inode, INODE_SIZE);                                                                // reading inode of root 
    fs_initialised = true;
    strcpy((char *)curr_dir.filename, "/");                                                                       // / means root                                      
    curr_dir.inode = 1;                                                                                           // setting current directory to root
    ls((char*) curr_dir.filename);                                                                                // Printing the contents of the root. 
    return 0;                                                           
  } else {
    if (!n1 || !n2){
      printf(" All arguments(path, number of inodes and total number of blocks) have not been entered\n");
      return 1;                                                                                                   // Added into provided code
    } else {
      numBlocks = atoi(n1);
      numInodes = atoi(n2);
      if( initfs(filepath,numBlocks, numInodes )){
        printf("The file system is initialized\n");
        fs_initialised = true;
        return 0;                                                                                                 // Added into provided code
      } else {
        printf("\033[1;31mError initializing file system. Exiting... \033[0m\n");
        return 1;
      }
    }
  }
}

// Initalises the filesystem 
int initfs(char* path, unsigned short blocks, unsigned short inodes) {
  unsigned int buffer[BLOCK_SIZE/4];
  int bytes_written;

  unsigned short i = 0;
  superBlock.fsize = blocks;
  unsigned short inodes_per_block= BLOCK_SIZE/INODE_SIZE;

  if((inodes%inodes_per_block) == 0) superBlock.isize = inodes/inodes_per_block;
  else superBlock.isize = (inodes/inodes_per_block) + 1;

  if((fileDescriptor = open(path,O_RDWR|O_CREAT,0700))== -1) {
    printf("\n open() failed with the following error [%s]\n", strerror(errno));
    return 0;
  }

  superBlock.nfree = 0;
  for (i = 0; i < FREE_SIZE; i++) superBlock.free[i] =  0;			                                                 // initializing free array to 0 to remove junk data.
  superBlock.ninode = I_SIZE;
  for (i = 0; i < I_SIZE; i++) superBlock.inode[i] = i + 1;		                                                   // Initializing the inode array to inode numbers

  superBlock.flock = 'a'; 					                                                                             // flock,ilock and fmode are not used.
  superBlock.ilock = 'b';
  superBlock.fmod = 0;
  superBlock.time[0] = ((unsigned)time(NULL)) & 0xffff;                                                          // superBlock modification time least significant bit
  superBlock.time[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;                                                  // superBlock modification time most significant bit

  lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET);
  write(fileDescriptor, &superBlock, BLOCK_SIZE);                                                                // writing superblock to file system

  for (i = 0; i < BLOCK_SIZE/4; i++) buffer[i] = 0;                                                              // writing zeroes to all inodes in ilist
  for (i = 0; i < superBlock.isize; i++)  write(fileDescriptor, buffer, BLOCK_SIZE);                             // writing buffer to superblock

  int data_blocks = blocks - 2 - superBlock.isize;
  int data_blocks_for_free_list = data_blocks - 1;

  create_root();                                                                                                 // Create root directory
  for ( i = 2 + superBlock.isize + 1; i < superBlock.fsize; i++ )  add_block_to_free_list(i , buffer);           // changed the condition from that provided

  superBlock.time[0] = ((unsigned)time(NULL)) & 0xffff;                                                          // superBlock modification time least significant bit
  superBlock.time[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;                                                  // superBlock modification time most significant bit
  return 1;
}

// Add Data blocks to free list
void add_block_to_free_list(int block_number,  unsigned int *empty_buffer){
  if ( superBlock.nfree == FREE_SIZE ) {
    int free_list_data[BLOCK_SIZE / 4], i;
    free_list_data[0] = FREE_SIZE;
    for ( i = 0; i < BLOCK_SIZE / 4; i++ ) free_list_data[i + 1] = ( i < FREE_SIZE ) ? superBlock.free[i] : 0;
    lseek( fileDescriptor, (block_number) * BLOCK_SIZE, 0 );
    write( fileDescriptor, free_list_data, BLOCK_SIZE );                                                          // Writing free list to header block
    superBlock.nfree = 0;
  } else {
	  lseek( fileDescriptor, (block_number) * BLOCK_SIZE, 0 );
    write( fileDescriptor, empty_buffer, BLOCK_SIZE );                                                            // writing 0 to remaining data blocks to get rid of junk data
  }
  superBlock.free[superBlock.nfree] = block_number;                                                               // Assigning blocks to free array
  ++superBlock.nfree;
}

// Get Free block from free list
int get_block_from_free_list(){
  int block_number = superBlock.free[--superBlock.nfree];                                                         // getting the free block number from free array          
  superBlock.time[0] = ((unsigned)time(NULL)) & 0xffff;                                                           // modification time least significant bit
  superBlock.time[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;                                                   // modification time most significant bit
  if (superBlock.nfree == 0) {                                                                                    // if nfree is zero we load the free array with data from data block at free[nfree]
    int free_list_data[BLOCK_SIZE / 4], i;
    lseek( fileDescriptor, (block_number) * BLOCK_SIZE, 0 );
    read( fileDescriptor, free_list_data, BLOCK_SIZE ); 
    superBlock.nfree = free_list_data[0];                                                                         // first value of the data block is nfree
    for ( i = 1; i < FREE_SIZE + 1 ; i++ ) {                                                                      // the next freesize values are the list of empty data blocks
      superBlock.free[i-1] = free_list_data[i];
    }
  }
  return block_number;
}

// Create root directory
void create_root() {
  int root_data_block = 2 + superBlock.isize;                                                                     // Allocating first data block to root directory

  curr_inode.flags = inode_alloc_flag | dir_flag | dir_large_file | dir_access_rights;   		                      // flag for root directory
  curr_inode.nlinks = 0;
  curr_inode.uid = 0;
  curr_inode.gid = 0;
  curr_inode.size = INODE_SIZE;
  for(int i = 1; i < ADDR_SIZE; i++ ) curr_inode.addr[i] = 0;
  curr_inode.addr[0] = root_data_block;
  curr_inode.actime[0] = ((unsigned)time(NULL)) & 0xffff;                                                         // access time least significant bit;
  curr_inode.actime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;                                                 // access time most significant bit
  curr_inode.modtime[0] = ((unsigned)time(NULL)) & 0xffff;                                                        // modification time least significant bit;
  curr_inode.modtime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;                                                // modification time most significant bit
  lseek(fileDescriptor, 2 * BLOCK_SIZE, 0);
  write(fileDescriptor, &curr_inode, INODE_SIZE);                                                                 // writing inode for root at 1 

  lseek(fileDescriptor, root_data_block * BLOCK_SIZE, 0);                                                         // changed code from one provided
  curr_dir.inode = 1;                                                                                             // root directory's inode number is 1.
  curr_dir.filename[0] = '.';                                                                                     
  curr_dir.filename[1] = '\0';
  write(fileDescriptor, &curr_dir, 16);                                                                           // root file . is at 1 
  curr_dir.filename[0] = '.';
  curr_dir.filename[1] = '.';
  curr_dir.filename[2] = '\0';
  write(fileDescriptor, &curr_dir, 16);                                                                           // root file .. is at 1 
}

// Function to copy a file in to v6 filesystem 
void cpin(char *fileSource){
  char* buffer[BLOCK_SIZE];           
  dir_type new_file, curr_dir_ref = curr_dir;
  inode_type curr_inode_ref = curr_inode;

  printf("Opening external file [%s]\n", fileSource);
  if(access(fileSource, F_OK) == -1) {                                                                            // the source file does not exist 
    printf("\033[1;31mThe file entered for copying doesn't exist.\033[0m\n");
    printf("\033[1;31m%s\033[0m\n", strerror(errno));
  } else{                                                                                                         // Checking whether the file exists 
    printf("The file exists.\n");
    int src_file = open(fileSource, O_RDONLY, 0700);                                                              // opening the external file to be copied
    int size = lseek(src_file, 0, SEEK_END);                                                                      // seek to end of file to get the size of the file
    printf("\033[1mThe size of external file to copy is [ %d ]\033[0m\n", size);                              
    lseek(src_file, 0, SEEK_SET);                                                                                 // seek back to beginning of file

    splitpath();                                                                                                  // seperating new file name and path to file 
    if(file.path != file.name && !cd(file.path)){                                                                 // if path & name are different and cd returns false then invalid path. 
      curr_dir = curr_dir_ref;                                                                                    // making curr dir as dir before cp 
      curr_inode = curr_inode_ref;                                                                                // making curr inode as inode before cp 
      return;                                                                                                     // return as error
    }

    bool correct_file;
    bool overwriting_file = false;
    do{
      searchSlice(curr_inode, file.name);
      if(slice.found){
        char input;                                                                                               // user input 
        printf("\033[1;31mFile already exists.\033[0m \n");
        printf("\033[1;36mDo you want to overwrite the file? \n");
        printf("[ y to overwrite / n to not overwrite / any other key to cancel copying ]: \033[0m ");
        scanf(" %c", &input);                                                                                     // TAKING input
        printf("\n");
        char user_input = tolower(input);                                                                         // lower case input
        if ('y' == user_input){                                                                                   // if user allows to over write then break and set the current slice and use it 
          printf("\033[1mOverwriting the existing file.\033[0m \n");
          overwriting_file = true;
          break;
        } else if('n' == user_input) {                                                                            // USER does not want to overwrite, taking a new file name. 
          char input[INPUT_SIZE];
          char *newName;
          printf("\033[1mTo avoid overwriting. Enter new file name:\n");
          scanf(" %[^\n]s", input);                                                                               // taking new file name
          newName = strtok(input," ");
          file.name = newName;                                                                                    // saving the name as the new file destination
          correct_file = false;
          continue;
        } else {                                                                                                  // user did not enter y or n so cancelling copy
          printf("\033[1;31mCancelling copy. Please wait...\033[0m");
          curr_dir = curr_dir_ref;                                                                                // making curr dir as dir before cp 
          curr_inode = curr_inode_ref;                                                                            // making curr inode as inode before cp 
          printf("\033[1;32mDone\033[0m\n");
          return;                                                                                                 // return as error
        }
      }
      correct_file = validateName();                                                                              // validating whether the file name entered is correct. 
    } while(!correct_file);

    curr_inode.actime[0] = ((unsigned)time(NULL)) & 0xffff;                                                       // access time least significant bit;
    curr_inode.actime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;                                               // access time most significant bit

    inode.flags = inode_alloc_flag | dir_access_rights;   		                                                    // inode flags for file
    inode.nlinks = 0;                                                                                             // 0 nlinks to file                   
    inode.uid = 0;
    inode.gid = 0;
    inode.size = size;                                                                                            // file size 
    inode.actime[0] = ((unsigned)time(NULL)) & 0xffff;                                                            // access time least significant bit;
    inode.actime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;                                                    // access time most significant bit
    inode.modtime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;                                                   // modification time most significant bit
    inode.modtime[0] = ((unsigned)time(NULL)) & 0xffff;                                                           // modification time least significant bit;
    
    // COPYING EXTERNAL FILE CONTENT TO INTERNAL FILE. 

    // if file is a multiple then we need file size / block size number iterations 
    // if file size is not a multiple, then the to account for the remainder 1 extra iteration is needed. 
    int itrs = (size % BLOCK_SIZE == 0) ?  size/BLOCK_SIZE:  (size / BLOCK_SIZE) + 1;                             // no. of iteration based on if the last data block is partially filled with data. 
    printf("Copying external file [%s] into the file system", fileSource);
    for(int i = 1; i <= itrs; i++){                                                                               // looping itrs number of times
        if (i*BLOCK_SIZE > size){                                                                                 // to put the last set of bytes which may be less than block size of file system
            read(src_file, buffer, size%BLOCK_SIZE);                                                              // writing remaining data into the buffer since the remaining part. 
            lseek(fileDescriptor, get_block_from_free_list()*BLOCK_SIZE, 0);                                      // getting next free block and lseek to that data block 
            write(fileDescriptor, buffer, size%BLOCK_SIZE);                                                       // writing the content of the buffer into the new internal file
        } else {
            read(src_file, buffer, BLOCK_SIZE);                                                                   // writing BLOCK_SIZE amount of data into the buffer. 
            lseek(fileDescriptor, get_block_from_free_list()*BLOCK_SIZE, 0);                                      // getting next free block and lseek to that data block
            write(fileDescriptor, buffer, BLOCK_SIZE);                                                            // writing the content of the buffer into the new internal file
        }
        inode.addr[i-1] = superBlock.free[superBlock.nfree];                                                      // saving data block address into the inode addr array
    }
    printf("\n\033[1;32mContent copied successfully.\033[0m\n");

    if(overwriting_file){
      lseek(fileDescriptor, 2*BLOCK_SIZE + INODE_SIZE*(slice.inodeNumber - 1), 0);                                // going to the inode 
      write(fileDescriptor, &inode, INODE_SIZE);                                                                  // writing the to the inode.                                                
    } else {
      lseek(fileDescriptor, 2*BLOCK_SIZE + INODE_SIZE*(superBlock.inode[--superBlock.ninode] - 1), 0);            // going to the inode 
      write(fileDescriptor, &inode, INODE_SIZE);                                                                  // writing the to the inode.                                                
      strcpy((char *) new_file.filename, file.name );
      new_file.inode = superBlock.inode[superBlock.ninode];
      lseek(fileDescriptor, curr_inode.addr[slice.addrIndex]*BLOCK_SIZE + 16*(slice.sliceIndex),0);               // going to the inode of the file
      write(fileDescriptor, &new_file, 16);                                                                       // writing the new file into the directory. 
    }
    curr_inode.modtime[0] = ((unsigned)time(NULL)) & 0xffff;                                                      // modification time least significant bit;
    curr_inode.modtime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;                                              // modification time most significant bit

    ls((char*) curr_dir.filename);                                                                                // printing contents of directory after copying
    curr_dir = curr_dir_ref;                                                                                      // making curr dir as dir before cp 
    curr_inode = curr_inode_ref;                                                                                  // making curr inode as inode before cp 
    return;                                                                                                       // return as error
  } 
}

// Function to copy a file out of v6 filesystem 
void cpout(char* fileDest){
  int fdes;
  dir_type curr_dir_ref = curr_dir;
  inode_type curr_inode_ref = curr_inode;
 
  splitpath();                                                                                                    // seperating new file name and path to file 
  if(file.path != file.name && !cd(file.path)){                                                                   // if path & name are different and cd returns false then invalid path. 
    curr_dir = curr_dir_ref;                                                                                      // making curr dir as dir before cp 
    curr_inode = curr_inode_ref;                                                                                  // making curr inode as inode before cp 
    return;                                                                                                       // return as error
  }

  searchSlice(curr_inode, file.name);                                                                             // search if file to copy exists in the path
  if (!slice.found) {                                                                                             // if not found then error
    printf("\033[;31mFile [ %s ] does not exist in [ %s ].\033[0m\n", file.name, file.path); 
    curr_dir = curr_dir_ref;                                                                                      // making curr dir as dir before cp 
    curr_inode = curr_inode_ref;                                                                                  // making curr inode as inode before cp 
    return;                                                                                                       // return as error
  }
  curr_inode.actime[0] = ((unsigned)time(NULL)) & 0xffff;                                                         // access time updated for directory least significant bit;
  curr_inode.actime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;                                                 // access time updated for directory most significant bit;
  printf("\033[;32m[ %s ] found successfully in [ %s ].\033[0m\n", file.name, file.path); 

  lseek(fileDescriptor, 2*BLOCK_SIZE + INODE_SIZE * (slice.inodeNumber - 1), 0);                                  // lseeking to inode block of file to copy
  read(fileDescriptor, &inode, INODE_SIZE);                                                                       // reading content of file inode. 

  if(access(fileDest, F_OK) != -1){                                                                               // if the file already exists with the destination name then it is overwritten
    printf("\033[;31mThe file already exists so will be overwritten.\033[0m\n");
    fdes = open(fileDest, O_RDWR | O_TRUNC, 0700);                                                                // Overwriting the existing file. 
  } else {                                                                                                        // if the file does not exists then it is created
    printf("\033[;32mThe file is created with read and write access.\033[0m\n");
    fdes = open(fileDest, O_RDWR | O_CREAT, 0700);                                                                // Creating a new file with r/w previledges. 
  }

  // if file size is a multiple of block size then all data blocks are completely filled and read 
  // if file size is not a multiple, then the remainder is in the last block which needs an extra iteration with diff buffer size. 
  int itrs =  (inode.size % BLOCK_SIZE == 0) ? inode.size / BLOCK_SIZE : (inode.size / BLOCK_SIZE) + 1;           // no. of iteration based on if the last data block is partially filled with data. 
  char complete_data_block_buffer[BLOCK_SIZE];                                                                    // buffer for the data blocks that are filled completely 
  char partial_data_block_buffer[inode.size % BLOCK_SIZE];                                                        // buffer for the last data block if partially filled. 
  printf("\033[1mCopying [%s] outside file system at [%s]\033[0m\n", file.name, fileDest); 

  // COPYING INTERNAL FILE CONTENT TO EXTERNAL FILE. 
  for (int i = 0; i<itrs; i++){
    if ((i+1)*BLOCK_SIZE > inode.size){                                                                           // if data block content > file size then block is not completely filled 
      lseek(fileDescriptor, inode.addr[i]*BLOCK_SIZE, 0);                                                         // lseek to the data block
      read(fileDescriptor, partial_data_block_buffer, inode.size % BLOCK_SIZE);                                   // read the data block content to partial_data_block_buffer 
      write(fdes, partial_data_block_buffer, inode.size % BLOCK_SIZE);                                            // write the partial_data_block_buffer content to new file
    } else {                                                                                                      // if data block content < file size then block is completely filled 
      lseek(fileDescriptor, inode.addr[i]*BLOCK_SIZE, 0);                                                         // lseek to the data block
      read(fileDescriptor, complete_data_block_buffer, BLOCK_SIZE);                                               // read the data block content to complete_data_block_buffer 
      write(fdes, complete_data_block_buffer, BLOCK_SIZE);                                                        // write the complete_data_block_buffer content to new file
    }
  }
  inode.actime[0] = ((unsigned)time(NULL)) & 0xffff;                                                              // access time updated for file least significant bit
  inode.actime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff ;                                                     // access time updated for file most significant bit;
  lseek(fileDescriptor, 2*BLOCK_SIZE + INODE_SIZE * (slice.inodeNumber - 1), 0);                                  // lseeking to inode block of file to copy
  write(fileDescriptor, &inode, INODE_SIZE);                                                                      // writing content to file inode. 
  printf("\033[1mCopied [%s] successfully\033[0m\n", file.name); 

  ls((char*) curr_dir.filename);                                                                                  // printing the contents of dir where file was copied out
  curr_dir = curr_dir_ref;                                                                                        // making curr dir as dir before cp 
  curr_inode = curr_inode_ref;                                                                                    // making curr inode as inode before cp 
  return;                                                                                                         // return as error
}

// Function to make new directories
void mkdir(){
  dir_type new_dir;                                                                                               // store new directory content   
  bool directory_found;                                                                                           // storing validity of path
  dir_type curr_dir_ref = curr_dir;                                                                               // storing the reference of the current directory
  inode_type curr_inode_ref = curr_inode;                                                                         // storing the reference of the current inode

  splitpath();                                                                                                    // seperating new file name and path to file 
  if(file.path != file.name && !cd(file.path)){                                                                   // if path & name are different and cd returns false then invalid path. 
    curr_dir = curr_dir_ref;                                                                                      // making curr dir as dir before operation started
    curr_inode = curr_inode_ref;                                                                                  // making curr inode as inode before operation started 
    return;                                                                                                       // return as error
  }

  bool validName;
  do { 
    searchSlice(curr_inode, file.name);
    if (slice.found) {                                                                                            // if directory exists then rename
      char choice;                                                                                                // user input 
      printf("\033[1;31mDirectory with the same name already exists.\033[0m\n");
      printf("\033[1;36mDo you want to rename the directory? \n");
      printf("[ y to rename / any other key to cancel creation ]: \033[0m ");
      scanf(" %c", &choice);                                                                                      // TAKING input
      printf("\n");
      char user_input = tolower(choice);                                                                          // lower case input
      if ('y' == user_input){                                                                                     // user is renaming the directory 
        char input[INPUT_SIZE];
        char *newName;
        printf("\033[1mEnter new directory name:\033[0m\n");
        scanf(" %[^\n]s", input);                                                                                 // taking new file name
        newName = strtok(input," ");
        file.name = newName;                                                                                      // saving the name as the new file destination
        validName = false;
        continue;
      } else {                                                                                                    // user did not enter y or n so cancelling copy
        printf("\033[1;31mCancelling copy. Please wait...\033[0m");
        curr_dir = curr_dir_ref;                                                                                  // setting current directory to what it was at the start of operation  
        curr_inode = curr_inode_ref;                                                                              // setting current inode to what it was at the start of operation    
        printf("\033[1;32mDone\033[0m\n");
        return;                                                                                                   // returning the function due to duplication error.
      }
    } 
   validName = validateName();                                                                                    // check if name provided follows the constraints
  } while(!validName);
  printf("Creating [ %s ]\n", file.name);                                                                         // Directory does not already exist. creating it 

  // i-node details of the new directory
  inode.actime[0] = ((unsigned)time(NULL)) & 0xffff;                                                              // access time for new directory least significant bit;
  inode.actime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;                                                      // access time for new directory most significant bit
  inode.modtime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;                                                     // modification time for new directory most significant bit
  inode.modtime[0] = ((unsigned)time(NULL)) & 0xffff;                                                             // modification time for new directory least significant bit;
  inode.flags = inode_alloc_flag | dir_flag | dir_access_rights;   		                                            // flag for new directory
  inode.size = 0;                                                                                                 // empty directory
  for( int i = 0; i < ADDR_SIZE; i++ ) inode.addr[i] = 0;                                                         // Putting 0s in all addresses of inode
  inode.addr[0] = get_block_from_free_list();                                                                     // saving next free block for new dir addr in inode. 

  // Creating directory references and adding to current directory. 
  strcpy((char *)new_dir.filename, file.name);                                                                    // copying the file name provided to dir name 
  new_dir.inode = superBlock.inode[--superBlock.ninode];                                                          // getting an inode
  lseek(fileDescriptor, curr_inode.addr[slice.addrIndex]*BLOCK_SIZE + 16*(slice.sliceIndex),0);                   // lseek to the first block of the dir
  write(fileDescriptor, &new_dir, 16);                                                                            // writing the new directory into the parent directory
  
  lseek(fileDescriptor, inode.addr[0] * BLOCK_SIZE, 0);                                                           // lseeked to dir data to add slices of files. 
  printf("Creating a reference to itself in the directory.\n");
  new_dir.filename[0] = '.';                                                                                      // making entry in dir for .
  new_dir.filename[1] = '\0';
  new_dir.inode = superBlock.inode[superBlock.ninode];                                                            // . points to new directory's inode number
  write(fileDescriptor, &new_dir, 16); 
  printf("Creating a reference to parent in the directory.\n");
  new_dir.filename[0] = '.';                                                                                      // making entry in dir for ..
  new_dir.filename[1] = '.';
  new_dir.filename[2] = '\0';
  new_dir.inode = curr_dir.inode;                                                                                 // .. points to parent directory's inode number
  write(fileDescriptor, &new_dir, 16);

  // writing the new dirrectory inode
  lseek(fileDescriptor, 2*BLOCK_SIZE + INODE_SIZE*(superBlock.inode[superBlock.ninode] - 1), 0);
  write(fileDescriptor, &inode, INODE_SIZE);                                                                      // writing the new dir inode.   

  printf("\033[1;32mSuccessfully created [ %s ]\033[0m\n", file.name);
  ls((char*) curr_dir.filename);                                                                                  // printing the content of the current directory
  curr_dir = curr_dir_ref;                                                                                        // setting current directory to what it was at the start of operation  
  curr_inode = curr_inode_ref;                                                                                    // setting current inode to what it was at the start of operation                                                                 
}

// Function to change directory
bool cd(char *directory_path){
  char delim[] = "/";                                                                                             // if / is encountered then directory level changes
  char* path = strtok(directory_path, delim);                                                                     // tokenising with / to split into various directory levels. 
  if (directory_path[0] == '/' || strcmp(directory_path, "") == 0) {                                              // absolute path - set curr inode to root inode 
    lseek(fileDescriptor, 2*BLOCK_SIZE, 0);                                                                       // lseeking to root inode
    read(fileDescriptor, &curr_inode, INODE_SIZE);                                                                // reading root inode 
    curr_dir.inode = 1;                                                                                           // set curr inode to root inode
    strcpy((char *)curr_dir.filename, "/");                                                                       // filename for root is / 
  } else {                                                                                                        // relative path 
    lseek(fileDescriptor, 2*BLOCK_SIZE + INODE_SIZE*(curr_dir.inode - 1), 0);                                     // lseeking to current directory inode
    read(fileDescriptor, &curr_inode, INODE_SIZE);                                                                // reading current directory inode  
  }
  while(path != NULL) {
    char* temp = path;                                                                                            // taking a copy of path
    path = strtok(NULL, delim);                                                                                   // creating next token in path by / 
    searchSlice(curr_inode, temp);                                                                                // searching if file exists 
    if (slice.found) {                                                                                            // if directory is found. Path is valid so far
      lseek(fileDescriptor, 2*BLOCK_SIZE+INODE_SIZE*(slice.inodeNumber-1), 0);                                    // lseeking to inode num's inode
      read(fileDescriptor, &curr_inode, INODE_SIZE);                                                              // reading the inode   
      printf("\033[1;32mSuccessfully located [ %s ]\033[0m\n", temp);
      curr_dir.inode = slice.inodeNumber;                                                                         // setting current directory as the one we are in
      strcpy((char *)curr_dir.filename, temp);                                                                    // copying filename to current directory
    } else {                                                                                                      // the directory not found. Path is invalid
      printf("\033[1;31mInvalid path. [ %s ] not found\033[0m\n", temp);
      return false;                                                                                               // returning false if path is invalid
    }
  }
  printf("\033[1;32mSuccessfully moved to [ %s ]\033[0m\n", curr_dir.filename);
  ls((char*) curr_dir.filename);                                                                                  // printing content of current folder
  return true;                                                                                                    // returning true if successful
}

// Function to remove directory or file
void rm(){
  dir_type curr_dir_ref = curr_dir;                                                                               // storing reference for the current directory
  unsigned int buffer[BLOCK_SIZE/4];                                                                              // buffer to add to the emptied data block
  dir_type empty_directory;                                                                                       // directory replace the removed file/directory   
  empty_directory.inode = 0;
  inode_type deleting_inode, curr_inode_ref = curr_inode;                                                         // storing inode to the deleted and reference for the current inode
   
  for (int i = 0; i < BLOCK_SIZE/4; i++) buffer[i] = 0;                                                           // writing zeroes into the buffer

  splitpath();                                                                                                    // splitting path into path and file/directory name

  if (strcmp(file.name, (char*)curr_dir.filename) == 0){                                                                             // quit if we trying to delete the directory we are in
    printf("\033[1;31mCan not delete as you are in the folder.\033[0m\n");
    return;
  }

  if(file.path != file.name && !cd(file.path)) {                                                                  // if path & name are different and cd returns false then invalid path. 
    curr_dir = curr_dir_ref;                                                                                      // making curr dir as dir before operation started
    curr_inode = curr_inode_ref;                                                                                  // making curr inode as inode before operation started 
    return;                                                                                                       // return as error
  }

  searchSlice(curr_inode, file.name);                                                                             //searching the file/dirrectory to be removed in the path
  if(!slice.found){                                                                                               // if file/director isn't found 
    printf("\033[1;31mInvalid path. [ %s ] does not exist\033[0m\n", file.name);                                                        
    curr_dir = curr_dir_ref;                                                                                      // making curr dir as dir before operation started
    curr_inode = curr_inode_ref;                                                                                  // making curr inode as inode before operation started 
    return;                                                                                                       // return as error
  }

  lseek(fileDescriptor,  2*BLOCK_SIZE + INODE_SIZE*(slice.inodeNumber - 1), 0);                                   // lseeking to the inode of the file/directory to be deleted
  read(fileDescriptor, &deleting_inode, INODE_SIZE);                                                              // reading the inode to be deleted

  if(isDirectory(deleting_inode.flags)){                                                                          // checking the the inode is for a file or dirrectory
    if (isEmptyDirectory(deleting_inode)) {                                                                       // if inode is of a directory then checking if directory is empty
      printf("\033[1;32m[ %s ] is an empty directory Deleting it!\033[0m\n", file.name);
    } else {                                                                                                      // if inode is of a directory but it isn't empty
      printf("\033[1;31m[ %s ] is a directory but it is not empty. Can not remove it!\033[0m\n", file.name);
      curr_dir = curr_dir_ref;                                                                                    // making curr dir as dir before operation started
      curr_inode = curr_inode_ref;                                                                                // making curr inode as inode before operation started 
      return;                                                                                                     // return as error
    } 
  } 
    
  int addr_index = 0;
  while(deleting_inode.addr[addr_index] != 0){                                                                    // clearing the occupied data blocks
      add_block_to_free_list(deleting_inode.addr[addr_index], buffer);
      addr_index++;
  }
  deleting_inode.flags = 0;                                                                                       // making the inode free but setting flag to zero 
  lseek(fileDescriptor,  2*BLOCK_SIZE + INODE_SIZE*(slice.inodeNumber - 1), 0);                                   // lseeking to the deleting file/directories inode to write free inode back                                
  write(fileDescriptor, &deleting_inode, INODE_SIZE);                                                             // writing free inode into its position

  lseek(fileDescriptor, BLOCK_SIZE*curr_inode.addr[slice.addrIndex] + 16*slice.sliceIndex,0);                     // lseeking to the directory/file record in the parent directory 
  write(fileDescriptor, &empty_directory, 16);                                                                    // making the space in parent directory free
  printf("\033[1;32m[ %s ] deleted successfully!\033[0m\n", file.name);

  ls((char*) curr_dir.filename);                                                                                  // printing the content of the current directory
  curr_dir = curr_dir_ref;                                                                                        // setting current directory to what it was at the start of operation  
  curr_inode = curr_inode_ref;                                                                                    // setting current inode to what it was at the start of operation  
}

// prints out the contents of the current inode if it is a directory. 
void ls(char *directory_path){
  dir_type dir_item;         
  int addrIndex = 0, x = (41-strlen(directory_path))/2;                                                           // counting number of =   
  for (int i=0; i<x; i++) printf("\033[1m=");                                                                     // for loop for first half of = 
  printf(" CONTENTS OF DIRECTORY [%s] ", directory_path);                                                         // heading 
  for (int i=0; i<x; i++) printf("=");                                                                            // for loop for second half of = 
  printf("\033[0m\n");                                                                                            // reset font setting 
  while (curr_inode.addr[addrIndex] != 0){
    lseek(fileDescriptor, curr_inode.addr[addrIndex]*BLOCK_SIZE, 0);                                              // lseek to start of directory 
    for (int count =1; count <= BLOCK_SIZE/16; count++){                                                          // looping through slices of directory. 
      read(fileDescriptor, &dir_item, 16);                                                                        // Reading the slice 
      if (dir_item.inode != 0)                                                                                    // print if inode is not 0
        printf("\033[1mInode\033[0m [ %d ] - \033[1mFilename\033[0m [ %s ]\n", dir_item.inode, dir_item.filename);
    }
    addrIndex++;                                                                                                  // increasing address index in addr array
  }
  curr_inode.actime[0] = ((unsigned)time(NULL)) & 0xffff;                                                         // access time of dir least significant bit;
  curr_inode.actime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;                                                 // access time of dir most significant bit

  lseek(fileDescriptor, 2*BLOCK_SIZE + INODE_SIZE*(curr_dir.inode- 1), 0);                                        // writing updated inode. 
  write(fileDescriptor, &curr_inode, INODE_SIZE);
  for (int i=0; i<((2*x)+26+strlen(directory_path)); i++) printf("\033[1m=");                                     // print division with = 
  printf("\033[0m\n");
}

// search directory for filename provided or get first empty slice if available in directory. 
void searchSlice(inode_type inode, char* fileDestination){ 
  dir_type curr_slice;
  int count = 1, available_slice = -1, addr_index = 0;
  while (inode.addr[addr_index] != 0){                                                                            // looping on the data block in the inode addr array
      lseek(fileDescriptor, inode.addr[addr_index]*BLOCK_SIZE, 0);                                                // lseeking to the data block of the address 
      while (count <= BLOCK_SIZE/16) {                                                                            // looping through slices of  directory to check duplicates don't occur
        read(fileDescriptor, &curr_slice, 16);                                                                    // reading the content of the slice with the filename and inodes
        if(curr_slice.inode == 0 && available_slice == -1) available_slice = count - 1;                           // if inode of slice is 0 and no available slice yet then set slice as available 
        else if(strcmp((char *)curr_slice.filename, fileDestination) == 0) {                                      // file name in the slice is same new file name. DUPLICATE ALERT
          slice.found = true;                                                                                     // slice with filename found 
          slice.addrIndex = addr_index;                                                                           // slice address of found file set 
          slice.sliceIndex = count - 1;                                                                           // index of the slice in addr
          slice.inodeNumber = curr_slice.inode;                                                                   // setting inode of slice to slice inode of same name in dir
          return;
        }
        ++count;                                                                                                  // increasing directory slice count
      }
      ++addr_index;                                                                                               // increase index in addr array
    }
    if ((count == BLOCK_SIZE/16) && available_slice == -1){                                                       // if count is complete but slice not found 
      curr_inode.addr[addr_index] = get_block_from_free_list();                                                   // get a new data block as the current one is full. 
      available_slice = 0;                                                                                        // slice available is 0th on new block
      ++addr_index;                                                                                 
    } 
    slice.found = false;                                                                                          // slice not found with filename
    slice.addrIndex = addr_index - 1;                                                                             // slice address set to first 0 address 
    slice.sliceIndex = available_slice;                                                                           // available slice number set 
    slice.inodeNumber = 0;                                                                                        // inode is 0
}

// checks whether this is a directory or plain text file
bool isDirectory(unsigned short n) { 
  unsigned short binaryNum[16];                                                                                   // array to store binary number 
  int i = 0;                                                                                                      // counter for binary array 
  for (;n > 0;n=n/2) {                                                                              
      binaryNum[i] = n % 2;                                                                                       // storing remainder in ith index of binary array 
      i++; 
  } 
  if (binaryNum[14] == 1 && binaryNum[13] == 0) return true;                                                      // 10 in flag value means directory otherwise not directory
  else return false;
} 

// separating path and name of file/directory
void splitpath(){
  int idx = strlen(file.path);                                                                                    // getting last index of file path
  file.name = file.path + idx;                                                                                    // moving the pointer to the end of the file path
  while (0 < idx && file.path[--idx] != '/');                                                                     // find the last occurence of / to get name of file or dir to remove
  if (file.path[idx] == '/') {
    file.name = file.path + idx + 1;                                                                              // moving the pointer of file name to last /'s index + 1
    file.path[idx] = '\0';                                                                                        // marking end of path to file at last / occurence
  } else {
    file.name = file.path;                                                                                        // no / means that item to remove is in the current directory
  }
}

// checks whether the directory pointed by inode is empty. 
bool isEmptyDirectory(inode_type inode) { 
  dir_type dir_item;
  int count = 1, addr_index = 0;
  while (addr_index < ADDR_SIZE){                                                                                 // looping on the inode addr array
    lseek(fileDescriptor, inode.addr[addr_index]*BLOCK_SIZE, 0);                                                  // lseeking to the data block of the addr[i] address
    while (count <= BLOCK_SIZE/16) {                                                                              // looping through each slice of the directory 
      read(fileDescriptor, &dir_item, 16);                                                                        // reading the filename and inodes of the slice
      if(strcmp((char *) dir_item.filename, ".\0") != 0                                                           // ignore slice for . 
      && strcmp((char *) dir_item.filename, "..\0") != 0                                                          // ignore slice for ..
      && strcmp((char *) dir_item.filename, "\0") != 0                                                            // ignore empty filenames 
      && dir_item.inode != 0){                                                                                    // if inode is 0 then not assigned
        printf("\033[1;31m[ %s ] found inside directory.\033[0m\n", dir_item.filename);
        return false;                                                                                             // Directory not empty.
      }
      ++count;                                                                                                    // increasing directory slice count
    }
    ++addr_index;                                                                                                 // increase index in addr array
  }
  return true;                                                                                                    // Directory is empty. 
} 

// user input file name validation check 
bool validateName(){
  if (strlen(file.name) >= 14){                                                                                   // if file name >= 14 then invalid name.
    printf("\033[1;31mFile name too long. \033[1;36mEnter new file name again: \033[m");
    scanf(" %[^\n]s", file.name);                                                                                 // taking user input for file name 
    return false;                                                                                                 // returning false as name is invalid
  }
  return true;                                                                                                    // returning true as name is valid 
}