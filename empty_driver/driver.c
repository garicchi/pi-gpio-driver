#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <asm/current.h>
#include <asm/uaccess.h>

MODULE_AUTHOR("Tarou Tnaka");
MODULE_LICENSE("Dual BSD/GPL");

#define DRIVER_NAME "hoge"

static const unsigned int minor_base = 0;
static const unsigned int minor_num = 2; // デバイスノードの数
static unsigned int major; // デバイスのメジャー番号(自動取得)

static struct cdev hoge_cdev; // キャラクタデバイス
static struct class *hoge_class = NULL; // udevが認識するためのデバイスクラス

// デバイスopen時の処理
static int hoge_open(struct inode *inode, struct file *file)
{
    printk("[%s] open", DRIVER_NAME);
    return 0;
}

// デバイスrelease時の処理
static int hoge_release(struct inode *inode, struct file *file)
{
    printk("[%s] release", DRIVER_NAME);
    return 0;
}

// デバイスread時の処理
static ssize_t hoge_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    printk("[%s] read", DRIVER_NAME);
    buf[0] = 'A';
    return 1;
}

// デバイスwrite時の処理
static ssize_t hoge_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    printk("[%s] write", DRIVER_NAME);
    return 1;
}

// 各システムコールのハンドラを構造体にまとめる
struct file_operations hoge_fops = {
    .open = hoge_open,
    .release = hoge_release,
    .read = hoge_read,
    .write = hoge_write,
};

// insmod時に呼ばれる初期化関数
static int hoge_init(void)
{
    printk("[%s] init", DRIVER_NAME);

    dev_t dev; // major番号とminor番号をまとめた構造体

    // メジャー番号を動的に確保する
    int r_alloc = alloc_chrdev_region(&dev, minor_base, minor_num, DRIVER_NAME);
    if (r_alloc < 0) {
        printk(KERN_ERR "alloc_chrdev_region = %d\n", r_alloc);
        return r_alloc;
    }
    major = MAJOR(dev); // major番号を取り出してstaticに保持しておく

    // 各ハンドラともにキャラクタデバイスを初期化する
    cdev_init(&hoge_cdev, &hoge_fops);
    hoge_cdev.owner = THIS_MODULE;

    // キャラクタデバイスドライバのカーネルへの登録
    dev = MKDEV(major, minor_base);
    int r_cdev = cdev_add(&hoge_cdev, dev, minor_num);
    if (r_cdev < 0) {
        printk(KERN_ERR "cdev_add = %d", r_cdev);
        unregister_chrdev_region(dev, minor_num);
        return r_cdev;
    }

    // udevが認識できるように、デバイスのクラス登録をする
    // /sys/class/hoge/ を作る
    hoge_class = class_create(THIS_MODULE, DRIVER_NAME);
    if (IS_ERR(hoge_class)) {
        printk(KERN_ERR "class_create");
        cdev_del(&hoge_cdev);
        unregister_chrdev_region(dev, minor_num);
        return -1;
    }

    // デバイスノードを作る
    // /sys/class/hoge/hoge* を作る
    for (int i = minor_base; i < (minor_base + minor_num); i++) {
        device_create(hoge_class, NULL, MKDEV(major, i), NULL, "hoge%d", i);
    }
    return 0;
}

// rmmod時に呼ばれる終了関数
static void hoge_exit(void)
{
    printk("[%s] exit", DRIVER_NAME);
    
    // デバイスノードを削除
    for (int i = minor_base; i < (minor_base + minor_num); i++) {
        device_destroy(hoge_class, MKDEV(major, i));
    }
    // デバイスクラスを削除
    class_destroy(hoge_class);

    // キャラクタデバイスドライバをカーネルから削除
    cdev_del(&hoge_cdev);

    // デバイスドライバのメジャーバージョンの削除
    unregister_chrdev_region(MKDEV(major, minor_base), minor_num);
}

module_init(hoge_init);
module_exit(hoge_exit);
