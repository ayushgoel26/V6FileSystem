/* Author: Ayush Goel , Reva Gupta 
* UTD ID: AXG190164, RXG190061
* CS 5348.001 Operating Systems
* Prof. S Venkatesan
* Project - 2 Part - 1*
***********
* Compilation: $ cc -o proj2part1.out fsaccess.c -std=c99
* Run using :  $ ./proj2part1.out
******************/

/***********************************************************************
 This program allows user to do two things:
   1. initfs: Initilizes the file system and redesigning the Unix file system to accept large
      files of up tp 4GB, expands the free array to 152 elements, expands the i-node array to
      200 elemnts, doubles the i-node size to 64 bytes and other new features as well.
   2. cpin : Copies a file in to the v6 file system 
   3. cpout : Copies a file out of the v6 file system
   4. h : prints all supported commands
   5. q: save all work and exit the program.

 User Input:
     - initfs (file path) (# of total system blocks) (# of System i-nodes)
     - cpin (external v6 file path) (filename in v6 root path)
     - cpout (filename in v6 root path) (external v6 file path)
     - h
     - q

 File name is limited to 14 characters.
 ***********************************************************************/

#include<stdio.h>
#include<stdbool.h>
#include<fcntl.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<stdlib.h>
#include<time.h>
#include<ctype.h>

#define FREE_SIZE 151                       // size of free array
#define I_SIZE 200                          // size of inode array
#define BLOCK_SIZE 1024                     // size of one Data block
#define ADDR_SIZE 11                        // size of address array in the inode array
#define INPUT_SIZE 256                      // max size string that user inputs 


// Superblock Structure

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

// I-Node Structure

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

inode_type inode;

typedef struct {
  unsigned short inode;
  unsigned char filename[14];
} dir_type;

dir_type root;

int fileDescriptor ;		                                            // file descriptor
bool fs_initialised = false;                                        // flag if filesystem is initialised or not
const unsigned short inode_alloc_flag = 0100000;                    // flag for if inode is allocated
const unsigned short dir_flag = 040000;                             // flag for if the inode is for directory
const unsigned short dir_large_file = 010000;                       // flag for if the directory/file is large
const unsigned short dir_access_rights = 000777;                    // User, Group, & World have all access privileges
const unsigned short INODE_SIZE = 64;                               // inode size


int initfs(char* path, unsigned short total_blcks,unsigned short total_inodes);       //function declaration for initfs
void add_block_to_free_list( int blocknumber , unsigned int *empty_buffer );          //fucntion declaration for add block to free list
void create_root();                                                                   //function declaration for creating root          
int preInitialization();                                                              //function declaration for pre-initialization
void cpin();                                                                          //function declaration for cpin 
void cpout();                                                                         //function declaration for cpout
int get_block_from_free_list();                                                       //function declaration for get block from free list

int main() {
  char input[INPUT_SIZE];
  char *splitter;
  unsigned int numBlocks = 0, numInodes = 0;
  char *filepath;
  printf("Size of super block = %lu , size of i-node = %lu\n", sizeof(superBlock), sizeof(inode));
  printf("Enter command:\n");
  while (1) {
    printf(">> ");
    scanf(" %[^\n]s", input);
    splitter = strtok(input, " ");
    if (strcmp(splitter, "initfs") == 0) {
      preInitialization();
      splitter = NULL;
    } else if (strcmp(splitter, "cpin") == 0) {
      if (!fs_initialised) printf("file system not initialised. Use initfs to initialise\n");
      else cpin();
      splitter = NULL;
    } else if (strcmp(splitter, "cpout") == 0) {
      if (!fs_initialised) printf("file system not initialised. Use initfs to initialise\n");
      else cpout();
      splitter = NULL;
    } else if (strcmp(splitter, "h") == 0) {
      printf( "----------------------------------------------------------------------------------------------------------\n"\
            " This program allows user to do two things:\n"\
            "   1. initfs: Initilizes the file system and redesigning the Unix file system to accept large\n"\
            "      files of up tp 4GB, expands the free array to 152 elements, expands the i-node array to\n"\
            "      200 elemnts, doubles the i-node size to 64 bytes and other new features as well.\n"\
            "   2. cpin : Copies a file in to the v6 file system \n"\
            "   3. cpout : Copies a file out of the v6 file system\n"\
            "   4. h : prints all supported commands\n"\
            "   5. q: save all work and exit the program.\n\n"\

            " User Input:\n"\
            "     - initfs (file path) (# of total system blocks) (# of System i-nodes)\n"\
            "     - cpin (external v6 file path) (filename in v6 root path)\n"\
            "     - cpout (filename in v6 root path) (external v6 file path)\n"\
            "     - h\n"\
            "     - q\n\n"\
            " File name is limited to 14 characters.\n"\
            "----------------------------------------------------------------------------------------------------------\n");
    } else if (strcmp(splitter, "q") == 0) {
      lseek(fileDescriptor, BLOCK_SIZE, 0);
      write(fileDescriptor, &superBlock, BLOCK_SIZE);
      printf("Quitting!\n");
      return 0;
    } 
  }
}

int preInitialization(){
  char *n1, *n2, *filepath;
  unsigned int numBlocks, numInodes;

  // reading arguments of initfs
  filepath = strtok(NULL, " ");
  n1 = strtok(NULL, " ");
  n2 = strtok(NULL, " ");

  if(access(filepath, F_OK) != -1) {
    fileDescriptor = open(filepath, O_RDWR, 0700);
    if(fileDescriptor == -1){
        printf("\n filesystem already exists but open() failed with error [%s]\n", strerror(errno));
        return 1;
    }
    printf("filesystem already exists and the same will be used.\n");
    // Loading information from the existing file system  (not provided as starter code)
    
    // reading superblock
    lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET); 
    read(fileDescriptor, &superBlock, BLOCK_SIZE); 
   
    // reading inode of root 
    lseek(fileDescriptor, 2 * BLOCK_SIZE, 0); 
    read(fileDescriptor, &inode, INODE_SIZE);

    // Loaded existing file system information.
    lseek(fileDescriptor, inode.addr[0]*BLOCK_SIZE, 0);
    // Printing the contents of the root. 
    dir_type entry;
    for (int count =1; count <= BLOCK_SIZE/16; count++){
      read(fileDescriptor, &entry, 16);// READING THE SLICE  
      if (entry.inode != 0)
        printf("The directory has file at inode [ %d ] with name [ %s ]\n", entry.inode, entry.filename);
    }
    fs_initialised = true;
    return 0;                                                           //Added into provided code
  } else {
    if (!n1 || !n2){
        printf(" All arguments(path, number of inodes and total number of blocks) have not been entered\n");
        return 1;                                                   //Added into provided code
    } else {
        numBlocks = atoi(n1);
        numInodes = atoi(n2);
        if( initfs(filepath,numBlocks, numInodes )){
          printf("The file system is initialized\n");
          fs_initialised = true;
          return 0;                                                 //Added into provided code
        } else {
          printf("Error initializing file system. Exiting... \n");
          return 1;
        }
    }
  }
}

int initfs(char* path, unsigned short blocks,unsigned short inodes) {

   unsigned int buffer[BLOCK_SIZE/4];
   int bytes_written;

   unsigned short i = 0;
   superBlock.fsize = blocks;
   unsigned short inodes_per_block= BLOCK_SIZE/INODE_SIZE;

   if((inodes%inodes_per_block) == 0)
   superBlock.isize = inodes/inodes_per_block;
   else
   superBlock.isize = (inodes/inodes_per_block) + 1;

   if((fileDescriptor = open(path,O_RDWR|O_CREAT,0700))== -1)
       {
           printf("\n open() failed with the following error [%s]\n",strerror(errno));
           return 0;
       }

   for (i = 0; i < FREE_SIZE; i++)
      superBlock.free[i] =  0;			//initializing free array to 0 to remove junk data. free array will be stored with data block numbers shortly.

   superBlock.nfree = 0;
   superBlock.ninode = I_SIZE;

   for (i = 0; i < I_SIZE; i++)
	    superBlock.inode[i] = i + 1;		//Initializing the inode array to inode numbers

   superBlock.flock = 'a'; 					//flock,ilock and fmode are not used.
   superBlock.ilock = 'b';
   superBlock.fmod = 0;
   superBlock.time[0] = ((unsigned)time(NULL)) & 0xffff;                  // modification time least significant bit
   superBlock.time[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;        // modification time most significant bit
  

   lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET);
   write(fileDescriptor, &superBlock, BLOCK_SIZE); // writing superblock to file system

   // writing zeroes to all inodes in ilist
   for (i = 0; i < BLOCK_SIZE/4; i++)
   	  buffer[i] = 0;

   for (i = 0; i < superBlock.isize; i++)
   	  write(fileDescriptor, buffer, BLOCK_SIZE);

   int data_blocks = blocks - 2 - superBlock.isize;
   int data_blocks_for_free_list = data_blocks - 1;

   // Create root directory
   create_root();

   for ( i = 2 + superBlock.isize + 1; i < superBlock.fsize; i++ ) {         // changed the condition from that provided
      add_block_to_free_list(i , buffer);
   }

   superBlock.time[0] = ((unsigned)time(NULL)) & 0xffff;                  // modification time least significant bit
   superBlock.time[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;         // modification time most significant bit

   return 1;
}

// Add Data blocks to free list
void add_block_to_free_list(int block_number,  unsigned int *empty_buffer){

  if ( superBlock.nfree == FREE_SIZE ) {

    int free_list_data[BLOCK_SIZE / 4], i;
    free_list_data[0] = FREE_SIZE;

    for ( i = 0; i < BLOCK_SIZE / 4; i++ ) {
       if ( i < FREE_SIZE ) {
         free_list_data[i + 1] = superBlock.free[i];
       } else {
         free_list_data[i + 1] = 0; // getting rid of junk data in the remaining unused bytes of header block
       }
    }

    lseek( fileDescriptor, (block_number) * BLOCK_SIZE, 0 );
    write( fileDescriptor, free_list_data, BLOCK_SIZE ); // Writing free list to header block

    superBlock.nfree = 0;

  } else {

	  lseek( fileDescriptor, (block_number) * BLOCK_SIZE, 0 );
    write( fileDescriptor, empty_buffer, BLOCK_SIZE );  // writing 0 to remaining data blocks to get rid of junk data
  }

  superBlock.free[superBlock.nfree] = block_number;  // Assigning blocks to free array
  ++superBlock.nfree;
}


// Get Free block from free list
int get_block_from_free_list(){
  int block_number = superBlock.free[--superBlock.nfree];               // getting the free block number from free array          
  superBlock.time[0] = ((unsigned)time(NULL)) & 0xffff;                 // modification time least significant bit
  superBlock.time[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;         // modification time most significant bit
  if (superBlock.nfree == 0) {                                          // if nfree is zero we load the free array with data from data block at free[nfree]
    int free_list_data[BLOCK_SIZE / 4], i;
    lseek( fileDescriptor, (block_number) * BLOCK_SIZE, 0 );
    read( fileDescriptor, free_list_data, BLOCK_SIZE ); 
    superBlock.nfree = free_list_data[0];                                // first value of the data block is nfree
    for ( i = 1; i < FREE_SIZE + 1 ; i++ ) {                             // the next freesize values are the list of empty data blocks
      superBlock.free[i-1] = free_list_data[i];
    }
  }
  return block_number;
}


// Create root directory
void create_root() {

  int root_data_block = 2 + superBlock.isize; // Allocating first data block to root directory
  int i;

  root.inode = 1;   // root directory's inode number is 1.
  root.filename[0] = '.';
  root.filename[1] = '\0';

  inode.flags = inode_alloc_flag | dir_flag | dir_large_file | dir_access_rights;   		// flag for root directory
  inode.nlinks = 0;
  inode.uid = 0;
  inode.gid = 0;
  inode.size = INODE_SIZE;
  inode.addr[0] = root_data_block;

  for( i = 1; i < ADDR_SIZE; i++ ) {
    inode.addr[i] = 0;
  }

  inode.actime[0] = ((unsigned)time(NULL)) & 0xffff;                   // access time least significant bit;
  inode.actime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;          // access time most significant bit
  inode.modtime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;         // modification time most significant bit
  inode.modtime[0] = ((unsigned)time(NULL)) & 0xffff;                  // modification time least significant bit;


  lseek(fileDescriptor, 2 * BLOCK_SIZE, 0);
  write(fileDescriptor, &inode, INODE_SIZE);   

  lseek(fileDescriptor, root_data_block * BLOCK_SIZE, 0);     // changed code from one provided
  write(fileDescriptor, &root, 16);

  root.filename[0] = '.';
  root.filename[1] = '.';
  root.filename[2] = '\0';

  write(fileDescriptor, &root, 16);

}

// Function to copy a file in to v6 filesystem 
void cpin(){
  printf("-------------------------------------------------------------------------------\n");
  // Initialising required variables 
  char *fileSource, *fileDestination, buffer[BLOCK_SIZE];
  inode_type inode_root;
  dir_type root_content_slice, new_file;

  // reading user inputs 
  fileSource = strtok(NULL, " ");                                     // reading the source file to copy inside the file system
  fileDestination = strtok(NULL, " ");                                // reading name of the destination file
  
  // sanity check of command 
  if (fileDestination == NULL || fileSource == NULL){
    printf("Missing parameters for cpin. Check help(h) for more info. \n");
    printf("-------------------------------------------------------------------------------\n");
    return;
  }

  printf("[INFO] Opening external file [%s]\n", fileSource);
  if(access(fileSource, F_OK) == -1) {                                // error when trying to access the source file. does not exist 
    printf("[ERROR] The file entered for copying doesn't exist.\n");
    printf("[ERRPR] %s\n", strerror(errno));
  } else{                                                           // Checking whether the file exists 
    printf("[DEBUG] The file exists.\n");
    int src_file = open(fileSource, O_RDONLY, 0700);                      // opening the external file to be copied
    int size = lseek(src_file, 0, SEEK_END);                              // seek to end of file to get the size of the file
    printf("[INFO] The size of external file = %d\n", size);                              
    lseek(src_file, 0, SEEK_SET);                                         // seek back to beginning of file
    inode.flags = inode_alloc_flag | dir_access_rights;   		        // inode flags for file
    inode.nlinks = 0;                                                 // 0 nlinks to file                   
    inode.uid = 0;
    inode.gid = 0;
    inode.size = size;                                                 // file size 
    
    // COPYING EXTERNAL FILE CONTENT TO INTERNAL FILE. 

    // itrs is used to decide how many iteration are run based on whether the file size is a multiple of block size 
    // if file is a multiple then we need file size / block size number iterations 
    // if file size is not a multiple, then the to account for the remainder 1 extra iteration is needed. 
    int itrs = (size % BLOCK_SIZE == 0) ?  size/BLOCK_SIZE:  (size / BLOCK_SIZE) + 1;

    printf("[INFO] Copying external file [%s] into the file system\n", fileSource);
    for(int i = 1; i <= itrs; i++){ // looping itrs number of times 
        if (i*BLOCK_SIZE > size){    // to put the last set of bytes which may be less than block size of file system
            read(src_file, buffer, size%BLOCK_SIZE); // writing remaining data into the buffer since the remaining part. 
            lseek(fileDescriptor, get_block_from_free_list()*BLOCK_SIZE, 0); // getting next free block and lseek to that data block 
            write(fileDescriptor, buffer, size%BLOCK_SIZE); // writing the content of the buffer into the new internal file
            printf("[DEBUG] Added into block %d\n", superBlock.free[superBlock.nfree]);
        } else {
            read(src_file, buffer, BLOCK_SIZE); // writing BLOCK_SIZE amount of data into the buffer. 
            lseek(fileDescriptor, get_block_from_free_list()*BLOCK_SIZE, 0); // getting next free block and lseek to that data block
            write(fileDescriptor, buffer, BLOCK_SIZE); // writing the content of the buffer into the new internal file
            printf("[DEBUG] Added into block %d\n", superBlock.free[superBlock.nfree]);
        }
        inode.addr[i-1] = superBlock.free[superBlock.nfree]; // saving data block address into the inode addr array
    }
    printf("[INFO] Content copied successfully. \n");

    inode.actime[0] = ((unsigned)time(NULL)) & 0xffff;                  // access time least significant bit;
    inode.actime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;         // access time most significant bit
    inode.modtime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;        // modification time most significant bit
    inode.modtime[0] = ((unsigned)time(NULL)) & 0xffff;                  // modification time least significant bit;


    lseek(fileDescriptor, 2*BLOCK_SIZE, 0);  // lseeking to the first inode block
    read(fileDescriptor, &inode_root, INODE_SIZE); // reading the first inode to get to the root. 
    
    int addr_index = 0; // index of the inode addr array
    int available_slice = 0;   // slice where file can be added 
    bool flag_duplicate_present = false; // flag if destination file exists or not
    bool file_overwrite_flag = false; // if destination file exists then flag whether to overwrite or not
    bool file_overwrite_cancel = false; // flad if we cancel the write
    int count = 1;
    while (inode_root.addr[addr_index] != 0){ // looping on the data block in the inode addr array
      printf("[DEBUG] The root is at block number [%d]\n", inode_root.addr[0]);
      lseek(fileDescriptor, inode_root.addr[addr_index]*BLOCK_SIZE, 0); // lseeking to the data block of the address 
      while (count <= BLOCK_SIZE/16) { // looping through each slice of the directory to make sure duplicates dont occur
        read(fileDescriptor, &root_content_slice, 16); // reading the content of the slice with the filename and inodes.
        if(root_content_slice.inode == 0 && available_slice == 0)  
          available_slice = count; // if inode of the slice is 0 and available slice is 0 then make this slice as available slice 
        else if(strcmp((char *)root_content_slice.filename,fileDestination) == 0) { // file name in the slice is same new file name. DUPLICATE ALERT
          char input; // user input 
          flag_duplicate_present = true;  // DUPLICATE name flag set 
          printf("[INFO] File already exists.\n");
          printf("Do you want to overwrite the file? [ y to overwrite / n to not overwrite / any other key to cancel copying ]:  ");
          scanf(" %c", &input); // TAKING input
          char user_input = tolower(input); // lower case input
          if ('y' == user_input){ // if user allows to over write then break and set the current slice and use it 
            printf("\n[INFO] Overwriting the existing file.\n");
            available_slice = count;
            file_overwrite_flag = true; // set flag of overwrite to true
            break;
          } else if('n' == user_input) { // USER does not want to overwrite, taking a new file name. 
              char input[INPUT_SIZE];
              char *newName;
              printf("To avoid overwriting. Enter new file name:\n");
              scanf(" %[^\n]s", input); // taking new file name
              newName = strtok(input," ");
              strcpy(fileDestination, newName); // saving the name as the new file destination
          } else { // user did not enter y or n so cancelling copy
              printf("[INFO] Cancelling copy. Please wait....\n");
              file_overwrite_cancel = true;         // set flag of overwrite cancel to true
              unsigned int buffer[BLOCK_SIZE/4];
              for (int i = 0; i < BLOCK_SIZE/4; i++)
                buffer[i] = 0; 
              for (int i=0; i < itrs; i++){
                lseek(fileDescriptor, superBlock.free[superBlock.nfree]*BLOCK_SIZE,0); // emptying data blocks
                write(fileDescriptor, buffer,0);  // removing data from data blocks
                ++superBlock.nfree; // increasing nfree because we got a new block for this file which is not needed
              }
              break;
          }
        }
        ++count; // increasing directory slice count
      }
      ++addr_index; // increase index in addr array
    }

    inode_root.actime[0] = ((unsigned)time(NULL)) & 0xffff;                  // access time least significant bit;
    inode_root.actime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;         // access time most significant bit


    // if count is complete but no slice is found then assign new block to root as the current one is full. 
    if ((count == BLOCK_SIZE/16) && available_slice == 0){
      inode_root.addr[addr_index] = get_block_from_free_list();
      ++addr_index;
      available_slice = 1;
    } 

    int free_inode;
    if (!flag_duplicate_present || !file_overwrite_flag){ // if no duplicate found  and overwrite not allowed then take a new inode 
      free_inode = superBlock.inode[--superBlock.ninode];  
    } else if (flag_duplicate_present && file_overwrite_flag){ // if duplicate found and overwrite is allowed then use existing one's inode 
      free_inode = root_content_slice.inode;
    } 

    if (!file_overwrite_cancel){
      lseek(fileDescriptor, 2*BLOCK_SIZE + INODE_SIZE*(free_inode - 1), 0);
      write(fileDescriptor, &inode, INODE_SIZE);

      // if file name >= 14 then take a new name. 
      while(strlen(fileDestination) >= 14){
        char input[INPUT_SIZE];
        char *splitter;
        printf("File name too long. Enter new file name again.\n");
        scanf(" %[^\n]s", input);
        splitter = strtok(input," ");
        strcpy(fileDestination,splitter);
      } 

      // copying file name to inode 
      strcpy((char *)new_file.filename, fileDestination );
      new_file.inode = free_inode;
      lseek(fileDescriptor, inode_root.addr[addr_index - 1]*BLOCK_SIZE + 16*(available_slice-1),0); // goingto the inode 
      write(fileDescriptor, &new_file, 16); // writing the new file into the directory. 

      inode_root.modtime[0] = ((unsigned)time(NULL)) & 0xffff;                  // modification time least significant bit;
      inode_root.modtime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;         // modification time most significant bit
    }

    
    // Printing the contents of the root. 
    lseek(fileDescriptor, inode_root.addr[addr_index - 1]*BLOCK_SIZE, 0); // lseek to start of root 
    for (int count =1; count <= BLOCK_SIZE/16; count++){
      read(fileDescriptor, &root_content_slice, 16);// READING THE SLICE  
      if (root_content_slice.inode != 0)
        printf("The directory has file at inode [ %d ] with name [ %s ]\n", root_content_slice.inode, root_content_slice.filename);
    }
    inode_root.actime[0] = ((unsigned)time(NULL)) & 0xffff;                  // access time least significant bit;
    inode_root.actime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;         // access time most significant bit
  } 
  printf("-------------------------------------------------------------------------------\n");
}

// Function to copy a file out of v6 filesystem 
void cpout(){
  printf("-------------------------------------------------------------------------------\n");
  //initialising variables needed.
  char *fileSource, *fileDestination;
  int fdes;
  dir_type root_dir_file;
  inode_type file_inode;

  // reading the file to copy out and the name of file to copy to. 
  fileSource = strtok(NULL, " ");
  fileDestination = strtok(NULL, " ");
  // sanity check of command 
  if (fileDestination == NULL || fileSource == NULL){
    printf("Missing parameters for cpout. Check help(h) for more info. \n");
    printf("-------------------------------------------------------------------------------\n");
    return;
  }
  // fetching the block number of root from its inode. 
  lseek(fileDescriptor, 2*BLOCK_SIZE, 0);
  read(fileDescriptor, &inode, INODE_SIZE);
  printf("[DEBUG] The root is at data block number - %d\n", inode.addr[0]);

  // searching for file in root directory
  printf("[DEBUG] Searching for file in directory\n");
  lseek(fileDescriptor, inode.addr[0]*BLOCK_SIZE, 0); // lseeking to the root. 
  bool file_found = false; // flag for file found in dir or not
  int count = 0;
  while (!file_found && count < (BLOCK_SIZE/16)) {
    read(fileDescriptor, &root_dir_file, 16); // reading content of dir slice by slice
    if (strcmp((char *) root_dir_file.filename, fileSource) == 0) file_found = true; // if file is found
    ++count; // increasing count of slices
  }

  inode.actime[0] = ((unsigned)time(NULL)) & 0xffff;                  // access time least significant bit;
  inode.actime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff;         // access time most significant bit


  if (!file_found) // if file not found
    printf("[ERROR] The file [%s] does not exist.\n", fileSource); 
  else { // if file is found 
    printf("[INFO] The file [%s] found successfully.\n", fileSource); 

    // fetching the inode of the file to copy out of filesystem
    lseek(fileDescriptor, 2*BLOCK_SIZE + INODE_SIZE * (root_dir_file.inode - 1), 0); // lseeking to inode block
    read(fileDescriptor, &file_inode, INODE_SIZE); // reading content into struct. 

    // Checking if there is a file which already exists with the given destination name. 
    // if it exists then it is overwritten otherwise it is created and written to.
    if(access(fileDestination, F_OK) != -1){ 
      printf("[DEBUG] The file already exists so will be overwritten.\n");
      fdes = open(fileDestination, O_RDWR | O_TRUNC, 0700); // Overwriting the existing file. 
    } else {
      printf("[DEBUG] The file is created with read and write access.\n");
      fdes = open(fileDestination, O_RDWR | O_CREAT, 0700); // Creating a new file and writing to it. 
    }

    // COPYING INTERNAL FILE CONTENT TO EXTERNAL FILE. 
    // itrs is used to decide how many iteration are run based on whether the last data block is partially filled with data. 
    // if file size is a multiple of block size then all data blocks are completely filled and read 
    // if file size is not a multiple, then the remainder is in the last block which needs an extra iteration with diff buffer size. 
    int itrs =  (file_inode.size % BLOCK_SIZE == 0) ? file_inode.size / BLOCK_SIZE : (file_inode.size / BLOCK_SIZE) + 1;
    char complete_data_block_buffer[BLOCK_SIZE]; // buffer for the data blocks that are filled completely 
    char partial_data_block_buffer[file_inode.size % BLOCK_SIZE]; // buffer for the last data block if partially filled. 

    printf("[INFO] Copying to external file at [%s]\n", fileDestination); 
    for (int i = 0; i<itrs; i++){
      if ((i+1)*BLOCK_SIZE > file_inode.size){ // if data block content is greater than file size then block is not completely filled 
        lseek(fileDescriptor, file_inode.addr[i]*BLOCK_SIZE, 0); //lseek to the data block
        read(fileDescriptor, partial_data_block_buffer, file_inode.size % BLOCK_SIZE); // read the data block content to partial_data_block_buffer 
        write(fdes, partial_data_block_buffer, file_inode.size % BLOCK_SIZE); // write the partial_data_block_buffer content to new file
      } else { // if data block content is less than file size then block is completely filled 
        lseek(fileDescriptor, file_inode.addr[i]*BLOCK_SIZE, 0); //lseek to the data block
        read(fileDescriptor, complete_data_block_buffer, BLOCK_SIZE); // read the data block content to complete_data_block_buffer 
        write(fdes, complete_data_block_buffer, BLOCK_SIZE); // write the complete_data_block_buffer content to new file
      }
    }
    file_inode.actime[0] = ((unsigned)time(NULL)) & 0xffff;                  // access time least significant bit
    file_inode.actime[1] = (((unsigned)time(NULL)) >> 16) & 0xffff ;       // access time most significant bit;
  }
  printf("-------------------------------------------------------------------------------\n");
}