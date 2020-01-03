/* Based on rectifation code:
 *  1) interceptor - https://bbs.archlinux.org/viewtopic.php?id=139406
 *  2) chardev2 -
 *  3) keysniffer - By Arun Prakash Jana <engineerarun@gmail.com>
 * */
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include "kci.h"
#include <asm/paravirt.h>
#include <asm/uaccess.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/keyboard.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/syscalls.h>

// Variable Declarations
static unsigned long	G_PROCESS_ID = -1;	// Process ID (-1 at init);
static unsigned long	G_FILE_DESCRIPTOR = -1;	// File descriptor (-1 at init);
static unsigned long	G_CIPHER_FLAG = 0;	// Cipher flag (0 at init), Saying whether the module should encrypt/decrypt data.
//
static size_t	buf_pos;
static char	keys_buf[BUF_LEN] = {0};
unsigned long	**sys_call_table;
unsigned long	original_cr0;
// Function Declarations
asmlinkage long new_read(int fd,char* __user buf,size_t count);
asmlinkage long new_write(int fd,char* __user buf,size_t count);
asmlinkage long (*ref_read)(int fd,char* __user buf,size_t count);
asmlinkage long (*ref_write)(int fd,char* __user buf,size_t count);
static ssize_t logs_read(struct file *filp,char *buffer,size_t len,loff_t *offset);
static long device_ioctl(struct file *file,unsigned int ioctl_num,unsigned long ioctl_param);
// Struct Declarations
const struct	file_operations logs_fops = {
			.owner = THIS_MODULE,
			.read = logs_read,
		};
static struct	dentry *file;			// Result from debugfs_create_file()
static struct	dentry *subdir;			// Result from debugfs_create_dir()
static struct	chardev_info device_info;
struct		chardev_info{spinlock_t lock;};
struct		file_operations Fops = { // Also avilable: (read,write,open,release,seek,readdir,select,mmap,flush)
			.unlocked_ioctl = device_ioctl,
		};
// Function
asmlinkage long new_read(int fd,char* __user buf,size_t count) { // Whenever read is invoked, the module should check whether the calling process ID and the file descriptor match those set in the variables. If so, the data should be read, decrypted, and then returned to the user. Otherwise, the call should remain as-is.
	long ret;
	ret = ref_read(fd,buf,count);
	if ((current->pid == G_PROCESS_ID)&&(fd == G_FILE_DESCRIPTOR)) {
		char logmsg[strlen(LOG_READ_MSG)+65];
		int i;
		int pointer;
		size_t len;
		if (G_CIPHER_FLAG == 1) {
			for (i=0;i<count;i++) {
				if (get_user(pointer,buf+i) < 0) {
					printk("error at get_user\n");
					return -1;
				}
				pointer -= 1;
				if (put_user(pointer,buf+i) < 0) {
					printk("error at put_user\n");
					return -1;
				}
			}
		}
		if (sprintf(logmsg,LOG_READ_MSG,G_PROCESS_ID,G_FILE_DESCRIPTOR,ret,count) < 0) { // sprintf(), If an output error is encountered, a negative value is returned.
			printk(F_ERROR_FUNCTION_SPRINTF_MSG);
			return -1;
		}
		len = strlen(logmsg);
		if ((buf_pos + len)>=BUF_LEN) {
			memset(keys_buf,0,BUF_LEN);
			buf_pos = 0;
		}
		strncpy(keys_buf+buf_pos,logmsg,len);
		buf_pos += len;
		keys_buf[buf_pos++] = '\n';
	}
	return ret;
}
asmlinkage long new_write(int fd,char* __user buf,size_t count) { // Whenever write is invoked, the module should check whether the calling process ID and the file descriptor match those set in the variables. If so, the data should be encrypted, and then written. Otherwise, the call should remain as-is.
	int i;
	int pointer;
	long ret;
	if ((current->pid == G_PROCESS_ID)&&(fd == G_FILE_DESCRIPTOR)) {
		char logmsg[strlen(LOG_READ_MSG)+65];
		size_t len;
		if (G_CIPHER_FLAG == 1) {
			write_cr0(original_cr0 & ~CRO_WP);
			for (i=0;i<count;i++) {
				if (get_user(pointer,buf+i) < 0) {
					printk("error at get_user\n");
					return -1;
				}
				pointer += 1;
				if (put_user(pointer,buf+i) < 0) {
					printk("error at put_user\n");
					return -1;
				}
			}
			write_cr0(original_cr0);
			ret =  ref_write(fd,buf,count);
			write_cr0(original_cr0 & ~CRO_WP);
			for (i=0;i<count;i++) {
				if (get_user(pointer,buf+i) < 0) {
					printk("error at get_user\n");
					return -1;
				}
				pointer -= 1;
				if (put_user(pointer,buf+i) < 0) {
					printk("error at put_user\n");
					return -1;
				}
			}
			write_cr0(original_cr0);
		}
		// Print to the log file
		if (sprintf(logmsg,LOG_WRITE_MSG,G_PROCESS_ID,G_FILE_DESCRIPTOR,ret,count) < 0) { // sprintf(), If an output error is encountered, a negative value is returned.
			printk(F_ERROR_FUNCTION_SPRINTF_MSG);
			return -1;
		}
		len = strlen(logmsg);
		if ((buf_pos + len)>=BUF_LEN) {
			memset(keys_buf,0,BUF_LEN);
			buf_pos = 0;
		}
		strncpy(keys_buf+buf_pos,logmsg,len);
		buf_pos += len;
		keys_buf[buf_pos++] = '\n';
	} else {
		ret =  ref_write(fd,buf,count);
	}
	return ret;
}

static ssize_t logs_read(struct file *filp,char *buffer,size_t len,loff_t *offset) {
	return simple_read_from_buffer(buffer,len,offset,keys_buf,buf_pos);
}
static long device_ioctl(struct file *file,unsigned int ioctl_num,unsigned long ioctl_param) { // Switch according to the ioctl called
	if (IOCTL_SET_PID == ioctl_num) { // Updates the PID variable to the given argument.
		G_PROCESS_ID = ioctl_param;
	} else if (IOCTL_SET_FD == ioctl_num) { // Updates the FD variable to the given argument.
		G_FILE_DESCRIPTOR = ioctl_param;
	} else if (IOCTL_CIPHER == ioctl_num) { // Sets the cipher flag to the given argument.
		if ((ioctl_param == 0)||(ioctl_param == 1)) {
			G_CIPHER_FLAG = ioctl_param;
		}
	}
	return SUCCESS;
}
static unsigned long **find_sys_call_table(void) {
	unsigned long int offset = PAGE_OFFSET;
	unsigned long **sct;
	while (offset < ULLONG_MAX) {
		sct = (unsigned long **)offset;
		if (sct[__NR_close] == (unsigned long *) sys_close) {
			return sct;
		}
		offset += sizeof(void *);
	}
	return NULL;
}
// Init Finction
static int __init kci_kmod_start(void) {
	// init dev struct
	memset(&device_info,0,sizeof(struct chardev_info));
	spin_lock_init(&device_info.lock);
	// Register a character device. Get newly assigned major num
	if ((register_chrdev(MAJOR_NUM,DEVICE_RANGE_NAME,&Fops)) < 0) { // Negative values signify an error
		printk(KERN_ALERT "%s failed with %d\n","Sorry, registering the character device ",MAJOR_NUM);
		return -1;
	}
	// Intercept syscall
	sys_call_table = find_sys_call_table();
	if (!(sys_call_table = find_sys_call_table())) {
		return -1;
	}
	original_cr0 = read_cr0();
	write_cr0(original_cr0 & ~CRO_WP);
	ref_read = (void *)sys_call_table[__NR_read];
	sys_call_table[__NR_read] = (unsigned long *)new_read;
	ref_write = (void *)sys_call_table[__NR_write];
	sys_call_table[__NR_write] = (unsigned long *)new_write;
	write_cr0(original_cr0);
	// Create debugfs log
	subdir = debugfs_create_dir(CONST_DEBUGFS_FOLDER,NULL);
	if (IS_ERR(subdir)) {
		return PTR_ERR(subdir);
	}
	if (!subdir) {
		return -ENOENT;
	}
	file = debugfs_create_file(CONST_DEBUGFS_FILE,S_IRUSR,subdir,NULL,&logs_fops);
	if (!file) {
		debugfs_remove_recursive(subdir);
		return -ENOENT;
	}
	buf_pos = 0;
	return 0;
}
static void __exit kci_kmod_end(void) {
	if (!sys_call_table) {
		return;
	}
	// Restore syscall
	write_cr0(original_cr0 & ~CRO_WP);
	sys_call_table[__NR_read] = (unsigned long *)ref_read;
	sys_call_table[__NR_write] = (unsigned long *)ref_write;
	write_cr0(original_cr0);
	msleep(2000);
	unregister_chrdev(MAJOR_NUM,DEVICE_RANGE_NAME); // Unregister the device should always succeed (didnt used to in older kernel versions)
	debugfs_remove_recursive(subdir);
}
// Init
module_init(kci_kmod_start);
module_exit(kci_kmod_end);
MODULE_LICENSE("GPL");

