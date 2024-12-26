#include <linux/cdev.h> 
#include <linux/device.h> 
#include <linux/fs.h> 
#include <linux/init.h> 
#include <linux/kernel.h> /* for sprintf() */ 
#include <linux/module.h> 
#include <linux/printk.h> 
#include <linux/version.h> 
#include <linux/utsname.h>
#include <linux/string.h>
#include <linux/sched.h>
#include "kfetch.h"

#define SUCCESS 0 
#define DEVICE_NAME "kfetch" /* Dev name as it appears in /proc/devices   */ 
#define BUF_LEN 512

#define CPUINFO_PATH "/proc/cpuinfo"
#define MODEL_NAME_FIELD "model name"

#define MEMINFO_PATH "/proc/meminfo"
#define UPTIME_PATH "/proc/uptime"

#define OFFSET 17
 
/* Global variables are declared as static, so are global within the file. */ 
 
static int major;

static struct class *cls; 

char *kfetch_buf = NULL;
static bool device_is_open = 0;
static int mask_info = -1;
 
/* Methods */ 
 
/* Called when a process tries to open the device file, like 
 * "sudo cat /dev/chardev" 
 */ 

static int device_open(struct inode *inode, struct file *file) 
{ 
    if (device_is_open) {
        pr_alert("Device is already open\n");
        return -EBUSY;
    }

    kfetch_buf = kmalloc(BUF_LEN, GFP_KERNEL);
    if (!kfetch_buf) {
        pr_alert("Failed to allocate memory\n");
        return -ENOMEM;
    }

    device_is_open = true;

    return 0; 
} 

 
/* Called when a process closes the device file. */ 
static int device_release(struct inode *inode, struct file *file) 
{
    kfree(kfetch_buf);
    device_is_open = false; 
    return 0;
} 
 
/* Called when a process, which already opened the dev file, attempts to 
 * read from it. 
 */ 

static ssize_t get_cpu_model_name(char *model_name, size_t length) {
    struct file *file;
    char buf[128];
    ssize_t ret, total = 0;
    char *model_line;

    file = filp_open(CPUINFO_PATH, O_RDONLY, 0);
    if (IS_ERR(file)) {
        pr_alert("Failed to open %s\n", CPUINFO_PATH);
        return -EFAULT;
    }

    while ((ret = kernel_read(file, buf, sizeof(buf) - 1, &file->f_pos)) > 0) {
        buf[ret] = '\0';
        model_line = strnstr(buf, MODEL_NAME_FIELD, ret);
        if (model_line) {
            int tt = sscanf(model_line, "%*s %*s %*s %128[^\n]", model_name);
            printk(KERN_INFO "tt: %d\n", tt);
            total += ret;
            break;
        }
        total += ret;
    }
    // printk(KERN_INFO "total: %ld\n", total);
    // printk(KERN_INFO "%s\n", model_line);
    // printk(KERN_INFO "%s\n", model_name);

    filp_close(file, NULL);

    return total;
}

static int get_free_memory(void) {
    struct file *file;
    char buf[128];
    ssize_t ret, total = 0;
    char *memory_line;
    char *free_memory = kmalloc(128, GFP_KERNEL);
    int memFree;

    file = filp_open(MEMINFO_PATH, O_RDONLY, 0);
    if (IS_ERR(file)) {
        pr_alert("Failed to open %s\n", MEMINFO_PATH);
        return -EFAULT;
    }

    while ((ret = kernel_read(file, buf, sizeof(buf) - 1, &file->f_pos)) > 0) {
        buf[ret] = '\0';
        memory_line = strnstr(buf, "MemFree:", ret);
        if (memory_line) {
            int tt = sscanf(memory_line, "%*s %s", free_memory);
            printk(KERN_INFO "tt: %d\n", tt);
            total += ret;
            break;
        }
        total += ret;
    }

    ret = kstrtoint(free_memory, 10, &memFree);

    filp_close(file, NULL);

    return memFree / 1024;
}

static int get_total_memory(void) {
    struct file *file;
    char buf[128];
    ssize_t ret, total = 0;
    char *memory_line;
    char *total_memory = kmalloc(128, GFP_KERNEL);
    int memTotal;

    file = filp_open(MEMINFO_PATH, O_RDONLY, 0);
    if (IS_ERR(file)) {
        pr_alert("Failed to open %s\n", MEMINFO_PATH);
        return -EFAULT;
    }

    while ((ret = kernel_read(file, buf, sizeof(buf) - 1, &file->f_pos)) > 0) {
        buf[ret] = '\0';
        memory_line = strnstr(buf, "MemTotal:", ret);
        if (memory_line) {
            int tt = sscanf(memory_line, "%*s %s", total_memory);
            printk(KERN_INFO "tt: %d\n", tt);
            total += ret;
            break;
        }
        total += ret;
    }
    ret = kstrtoint(total_memory, 10, &memTotal);
    filp_close(file, NULL);

    return memTotal / 1024;
}
static int get_uptime(void) {
    struct file *file;
    char buf[128] = {0};
    char line[128] = {0};
    ssize_t ret = 0;
    int uptime;
    int tt;

    file = filp_open(UPTIME_PATH, O_RDONLY, 0);
    if (IS_ERR(file)) {
        pr_alert("Failed to open %s\n", MEMINFO_PATH);
        return -EFAULT;
    }

    ret = kernel_read(file, buf, sizeof(buf) - 1, &file->f_pos);
    buf[ret] = '\0';
    tt = sscanf(buf, "%128[^.]", line);
    ret = kstrtoint(line, 10, &uptime);
    filp_close(file, NULL);

    return uptime / 60;
}

static ssize_t kfetch_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset) {
    char image[8][64] = 
    {   "                 ",
        "       .-.       ",
        "      (.. |      \n", 
        "      <>  |      \n",  
        "     / --- \\     \n",  
        "    ( |   | |    \n",
        "  |\\\\_)___/\\)/\\  \n", 
        " <__)------(__/  \n"  
    };

    int row = 0;

    char hostname[32] = {0};
    char dash[32] = {0};
    int dash_len;
    struct new_utsname *uts;
    uts = utsname();
    snprintf(hostname, sizeof(hostname), uts->nodename);
    strcat(hostname, "\n");
    strcat(image[row], hostname);
    row++;
    
    dash_len = strlen(hostname) - 1;
    for(int i = 0; i < dash_len; i++) {
        strcat(dash, "-");
    }
    strcat(dash, "\n");
    strcat(image[row], dash);
    row++;

    if (mask_info & KFETCH_RELEASE) {
        char release[32];
        char *target = strstr(image[row], "\n");
        *target = '\0';
        snprintf(release, sizeof(release), "Kernel:   %s\n", uts->release);
        strcat(image[row], release);
        row++;
    }

    if (mask_info & KFETCH_CPU_MODEL) {
        char cpu_model[128] = {0};
        char *target = strstr(image[row], "\n");
        *target = '\0';
        strcpy(cpu_model, "CPU:      ");
        get_cpu_model_name(cpu_model+10, sizeof(cpu_model));
        strcat(cpu_model, "\n"); 
        strcat(image[row], cpu_model);
        row++;
    }

    if (mask_info & KFETCH_NUM_CPUS) {
        char num_cpus[128] = {0};
        int online, total;
        char *target = strstr(image[row], "\n");
        *target = '\0';
        online = num_online_cpus();
        total = num_possible_cpus();
        snprintf(num_cpus, sizeof(num_cpus), "CPUs:     %d / %d\n", online, total);
        strcat(image[row], num_cpus);
        row++;
    }

    if (mask_info & KFETCH_MEM) {
        char memory[128] = {0};
        int free, total;
        char *target = strstr(image[row], "\n");
        *target = '\0';
        free = get_free_memory();
        total = get_total_memory();
        snprintf(memory, sizeof(memory), "Mem:      %d MB / %d MB\n", free, total);
        strcat(image[row], memory);
        row++;
    }

    if (mask_info & KFETCH_NUM_PROCS) {
        char procs[128] = {0};
        struct task_struct *task;
        int count = 0;
        char *target = strstr(image[row], "\n");
        *target = '\0';
        for_each_process(task) {
            count++;
        }
        snprintf(procs, sizeof(procs), "Procs:    %d\n", count);
        strcat(image[row], procs);
        row++;
    }
    
    if (mask_info & KFETCH_UPTIME) {
        char uptime[128] = {0};
        int minutes;
        char *target = strstr(image[row], "\n");
        *target = '\0';
        minutes = get_uptime();
        snprintf(uptime, sizeof(uptime), "Uptime:   %d mins\n", minutes);
        strcat(image[row], uptime);
        row++;
    }

    for(int i = 0; i < 8; i++) {
        printk(KERN_INFO "%s", image[i]);
        strcat(kfetch_buf, image[i]);
    }

    if (copy_to_user(buffer, kfetch_buf, strlen(kfetch_buf))) {
        pr_alert("Failed to copy data to user\n");
        return -EFAULT;
    }

    return strlen(kfetch_buf);  
}

static ssize_t kfetch_write(struct file *flip, const char __user *buffer, size_t  length, loff_t *offset)  {

    if (length > sizeof(mask_info)) {
        return -EINVAL;
    }
        
    if (copy_from_user(&mask_info, buffer, length)) {
        pr_alert("Failed to copy data from user\n");
        return -EFAULT;
    }

    return length;
}

static struct file_operations chardev_fops = {
    .owner = THIS_MODULE, 
    .read = kfetch_read, 
    .write = kfetch_write, 
    .open = device_open, 
    .release = device_release, 
}; 

static int __init chardev_init(void) 
{ 
    major = register_chrdev(0, DEVICE_NAME, &chardev_fops); 
 
    if (major < 0) { 
        pr_alert("Registering char device failed with %d\n", major); 
        return major; 
    } 
 
    pr_info("I was assigned major number %d.\n", major); 
 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0) 
    cls = class_create(DEVICE_NAME); 
#else 
    cls = class_create(THIS_MODULE, DEVICE_NAME); 
#endif 
    device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME); 
 
    pr_info("Device created on /dev/%s\n", DEVICE_NAME); 
 
    return SUCCESS; 
} 
 
static void __exit chardev_exit(void) 
{ 
    device_destroy(cls, MKDEV(major, 0)); 
    class_destroy(cls); 
 
    /* Unregister the device */ 
    unregister_chrdev(major, DEVICE_NAME); 
} 
 
module_init(chardev_init); 
module_exit(chardev_exit); 
 
MODULE_LICENSE("GPL");
