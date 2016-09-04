//---------------------------------------------------------------------------
// Copyright (c) 2016 Michael G. Brehm
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//---------------------------------------------------------------------------

//
// uapi-generic.c
//
// Translation Unit input file for tools.builduapi to generate uapi-generic.h
//

#include <linux/types.h>
#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/elf.h>
#include <linux/elf-em.h>
#include <linux/fs.h>
#include <linux/magic.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <asm/stat.h>
#include <asm/statfs.h>

// MS_PERMOUNT_MASK
//
// MS_XXXXX flags that apply to a mount rather than an entire file system
#define MS_PERMOUNT_MASK (MS_NODEV | MS_NOEXEC | MS_NOSUID | MS_NOATIME | MS_NODIRATIME | MS_RELATIME)

// include/linux/stat.h
//
#define S_IRWXUGO	(S_IRWXU | S_IRWXG | S_IRWXO)
#define S_IALLUGO	(S_ISUID | S_ISGID | S_ISVTX | S_IRWXUGO)
#define S_IRUGO		(S_IRUSR | S_IRGRP | S_IROTH)
#define S_IWUGO		(S_IWUSR | S_IWGRP | S_IWOTH)
#define S_IXUGO		(S_IXUSR | S_IXGRP | S_IXOTH)

// Additional UAPI Data Types
//
typedef __kernel_gid_t		gid_t;
typedef __kernel_mode_t		mode_t;
typedef __kernel_uid_t		uid_t;
typedef __kernel_fsid_t		fsid_t;

// todo: verify size, alignment and packing within Linux kernel
struct linux_dirent {

	unsigned long	d_ino;
	unsigned long	d_off;
	unsigned short	d_reclen;
	char			d_name[1];
};

// todo: verify size, alignment and packing within Linux kernel
struct linux_dirent64 {

	__u64			d_ino;
	__s64			d_off;
	unsigned short	d_reclen;
	unsigned char	d_type;
	char			d_name[0];
};