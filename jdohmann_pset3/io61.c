#include "io61.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/mman.h>
#include <errno.h>
#define BUFSIZE1 32768
#define BUFSIZE2 32768
// io61.c
//    YOUR CODE HERE!


// io61_file
//    Data structure for io61 file wrappers. Add your own stuff.

struct io61_file {
    int fd;
    unsigned char cbuf[BUFSIZE1];
    size_t cpos;
    size_t csz;

    off_t tag;
    
    int mode;
    int multiplier;
    
    int last_stride;
    int counter;
    int mmapped;
    
    int size;
    
    char* mmap;
    
    off_t offset;

    unsigned char outbuf[BUFSIZE2];
    size_t outsz;
    size_t outpos;
};



// io61_fdopen(fd, mode)
//    Return a new io61_file for file descriptor `fd`. `mode` is
//    either O_RDONLY for a read-only file or O_WRONLY for a
//    write-only file. You need not support read/write files.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);

    io61_file* f = (io61_file*) malloc(sizeof(io61_file));
    f->fd = fd;
    f->offset = 0;

   
    f->mode =  mode;
    
    
    

    int size = lseek(f->fd, 0, SEEK_END); // seek to end of file
    lseek(f->fd, 0, SEEK_SET); 
    f->size = size;
    if(size < BUFSIZE1)
    {
        f->multiplier = 8; //In order to avoid unnecessary overhead we shrink our buffer size for files smaller than the default, this also applies to pipes and other files that don't have a "size"
    }
    else
    {
        f->multiplier = 1;
    }
    
    
    return f;
}


// io61_close(f)
//    Close the io61_file `f` and release all its resources.

int io61_close(io61_file* f) {
    io61_flush(f);
    if(f->outpos != 0)
    {
        write(f->fd, f->outbuf, f->outpos);
    }
    int r = close(f->fd);
    if(f->mmapped && f->size > 0)
    {
        munmap(f->mmap, f->size);
    }

    free(f);
    return r;
}


// io61_readc(f)
//    Read a single (unsigned) character from `f` and return it. Returns EOF
//    (which is -1) on error or end-of-file.
int io61_readc_private(io61_file* f, int flag) 
{   
    
    
    if(f->mmapped && f->size > 0)
    {
        int ch = *(f->mmap + f->offset);
        f->offset++;
        return ch;
    }
    //We need to read in new data, if we try to read in more and its empty then we reached the EOF
    if(f->csz == f->cpos)
    {
        
        size_t size = read(f->fd, f->cbuf, BUFSIZE1 / f->multiplier);

        f->cpos = 0;
        f->csz = size;
        f->tag += size; //Update tag

    }

    if(flag)
    {
        if(f->csz != 0)
        {

            unsigned char ret = ((unsigned char *)f->cbuf)[f->cpos];
            f->cpos++;
            f->offset++;
            return (unsigned int) ret;
        }
        else
        {
            
            return EOF;   
        }
    }
    else
    {
        return f->csz;
    
    }
    
}

// io61_readc(f)
//    Read a single (unsigned) character from `f` and return it. Returns EOF
//    (which is -1) on error or end-of-file.
int io61_readc(io61_file* f) 
{   
    
   io61_readc_private(f, 1);
    
}

// io61_read(f, buf, sz)
//    Read up to `sz` characters from `f` into `buf`. Returns the number of
//    characters read on success; normally this is `sz`. Returns a short
//    count, which might be zero, if the file ended before `sz` characters
//    could be read. Returns -1 if an error occurred before any characters
//    were read.

ssize_t io61_read(io61_file* f, char* buf, size_t sz) 
{

    size_t nread = 0;
    

    if(f->mmapped && f->size > 0)
    {
        if(f->offset + sz > f->size)
        {
            memcpy(buf, f->mmap + f->offset, f->size - f->offset);
            nread = f->size - f->offset;

        }
        else
        {
            memcpy(buf, f->mmap + f->offset, sz);
            nread = sz;

        }
        f->offset+= nread;
        
    }
    else
    {
   
        if(f->cpos == f->csz)
        {
            (void)io61_readc_private(f, 0); //This way we only instantiate a new buffer and not read any of it
        }
        
        
        if(f->csz != 0) //If some chars were successfully read into the buffer
        {
            if(sz + f->cpos > f->csz) //We are asking for more than can be delivered with the given buffer
            {

                memcpy(buf, &f->cbuf[f->cpos], f->csz - f->cpos);
                
                nread = f->csz - f->cpos;
                f->cpos = f->csz;

            }
            else
            {
                memcpy(buf, &f->cbuf[f->cpos], sz);
                f->cpos += sz;    
                nread = sz;    
                
            }
            f->offset += nread;

        }
     }
     

     return nread;
        
  
    
    

    
}
/*

ssize_t io61_read(io61_file* f, char* buf, size_t sz) {
    size_t pos = 0;   // number of characters read so far
    while (pos != sz) {
        if (f->cpos != f->csz) {
            ssize_t n = sz - pos; //Num chars left to read
            if (n > f->csz - f->cpos)  //If number of chars left to read is greater than remaining chars in buffer then set our sights lower
                n = f->csz - f->cpos;  
            memcpy(&buf[pos], &f->cbuf[f->cpos], n);
            f->cpos += n;   // ****** Make sure you remembered these! //Update position in buffer
            pos += n;       // ****** //increment num read
        } else {
            f->cpos = f->csz = 0;   // mark cache as empty
            ssize_t n = read(f->fd, f->cbuf, BUFSIZ);
            if (n > 0)
                f->csz = n;
            else
                return pos ? pos : n;
        }
    }
    return pos;
}
*/
// io61_writec(f)
//    Write a single character `ch` to `f`. Returns 0 on success or
//    -1 on error.

int io61_writec(io61_file* f, int ch)
{
   /* unsigned char buf[1];
    buf[0] = ch;
    if (write(f->fd, buf, 1) == 1)
        return 0;
    else
        return -1;
   */
   
   if(f->outpos == f->outsz)
   {
        if(f->outpos != 0)
        {

            int nbytes = write(f->fd, f->outbuf, f->outpos);

            if(nbytes < BUFSIZE2)
            {
                return -1;
            }
        }
       
        

        f->outsz = BUFSIZE2;
        f->outpos = 0;
   }

   if(f->outpos < f->outsz)
   {
        f->outbuf[f->outpos] = (unsigned char)ch;
        f->outpos++;
   }
   
   
   
}


// io61_write(f, buf, sz)
//    Write `sz` characters from `buf` to `f`. Returns the number of
//    characters written on success; normally this is `sz`. Returns -1 if
//    an error occurred before any characters were written.

ssize_t io61_write(io61_file* f, const char* buf, size_t sz) 
{
    size_t nwritten = 0;
    while (nwritten != sz) {
        if (io61_writec(f, buf[nwritten]) == -1)
            break;
        ++nwritten;
    }
    if (nwritten != 0 || sz == 0)
        return nwritten;
    else
        return -1;
}


// io61_flush(f)
//    Forces a write of all buffered data written to `f`.
//    If `f` was opened read-only, io61_flush(f) may either drop all
//    data buffered for reading, or do nothing.

int io61_flush(io61_file* f) {
     if ((f->mode & O_ACCMODE) != O_RDONLY)
     {
     
        
        write(f->fd, f->outbuf, f->outpos);
        f->outpos = 0;
        f->outsz = 0;
        memset(f->outbuf, 0, BUFSIZE2);
      }
      else
      {
        return 0;
      }
   
}


// io61_seek(f, pos)
//    Change the file pointer for file `f` to `pos` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, off_t pos)
{
    if(f->mmapped && f->size > 0)
    {
        f->offset = pos;
       
    }
    else
    {
        
        int seek = pos - f->offset;
        f->offset = pos;

        if(seek < f->last_stride*1.15 && seek > f->last_stride*.85 && seek > BUFSIZE1) //Keep track of the number times consecutively someone seeks past the buffer size, this should hint at behavior which should be resolved by mmap
        {
            f->counter++;

            if(f->counter > 2)
            {
                f->mmapped = 1;
                f->mmap = (char *)mmap(NULL, f->size, PROT_READ, MAP_SHARED, f->fd, 0);
                return 0;
            }
        }
        else
        {
            f->counter = 0;
        }
        
        f->last_stride = seek;
        if ((f->mode & O_ACCMODE) != O_RDONLY)
        {
            io61_flush(f);
            
            off_t r = lseek(f->fd, pos, SEEK_SET);
            if (r != pos)
                  return -1;
            f->tag = pos;        
        }
        else if (pos >= f->tag && pos <= f->tag + f->csz) 
        {
                 f->cpos = pos - f->tag;             
                 return 0;
        }
        else
        {

                int x = pos;
                int y = BUFSIZE1;
                
                off_t aligned_off = x - (x % y);  
                
                off_t r = lseek(f->fd, aligned_off, SEEK_SET);

                if(r == aligned_off)
                {

                    
                    f->cpos = 0;
                    f->csz = 0;
                    io61_readc_private(f, 0);
                    f->tag = aligned_off; 
                    f->cpos = pos - f->tag;             
                   
                    return 0;   
                 }
                 else
                 {
                    return -1;
                 }     
           }
           

    }



    
    
    
   
    return 0;
}


// You shouldn't need to change these functions.

// io61_open_check(filename, mode)
//    Open the file corresponding to `filename` and return its io61_file.
//    If `filename == NULL`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != NULL` and the named file cannot be opened.

io61_file* io61_open_check(const char* filename, int mode) {
    int fd;
    
    if (filename)
        fd = open(filename, mode, 0666);
    else if ((mode & O_ACCMODE) == O_RDONLY)
        fd = STDIN_FILENO;
    else
        fd = STDOUT_FILENO;
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode & O_ACCMODE);
}


// io61_filesize(f)
//    Return the size of `f` in bytes. Returns -1 if `f` does not have a
//    well-defined size (for instance, if it is a pipe).

off_t io61_filesize(io61_file* f) {
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode))
        return s.st_size;
    else
        return -1;
}


// io61_eof(f)
//    Test if readable file `f` is at end-of-file. Should only be called
//    immediately after a `read` call that returned 0 or -1.

int io61_eof(io61_file* f) {
    char x;
    ssize_t nread = read(f->fd, &x, 1);
    if (nread == 1) {
        fprintf(stderr, "Error: io61_eof called improperly\n\
  (Only call immediately after a read() that returned 0 or -1.)\n");
        abort();
    }
    return nread == 0;
}
