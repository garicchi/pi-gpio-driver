#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <asm/io.h>

MODULE_AUTHOR("Tarou Tnaka");
MODULE_LICENSE("Dual BSD/GPL");

#define DRIVER_NAME "hoge"

static const unsigned int minor_base = 0;
static const unsigned int minor_num = 2; // デバイスノードの数
static unsigned int major; // デバイスのメジャー番号(自動取得)

static struct cdev hoge_cdev; // キャラクタデバイス
static struct class *hoge_class = NULL; // udevが認識するためのデバイスクラス

// raspberry piのメモリマップドI/Oアドレス
#define REG_ADDR_BASE 0x3F000000  // IOペリフェラルの開始アドレス
#define REG_ADDR_GPIO_BASE (REG_ADDR_BASE + 0x00200000) // GPIOの開始アドレス
#define REG_ADDR_GPIO_GPFSEL_0 0x0000  // GPIO Function Selectレジスタのアドレス
#define REG_ADDR_GPIO_OUTPUT_SET_0 0x001C  // GPIO Pin Setレジスタのアドレス
#define REG_ADDR_GPIO_OUTPUT_CLR_0 0x0028  // GPIO Pin Clearレジスタのアドレス
#define REG_ADDR_GPIO_LEVEL_0 0x0034  // GPIO Pin Levelレジスタのアドレス


static void set_register(unsigned int addr, unsigned int val)
{
    *((volatile unsigned int*)(addr)) = val;
}

static unsigned int get_register(unsigned int addr)
{
    return *((volatile unsigned int*)(addr));
}

static void print_register(unsigned int addr)
{
    unsigned int v = get_register(addr);
    printk("REG [0x%08X] = 0x%08X", addr, v);
}

// デバイスopen時の処理
static int hoge_open(struct inode *inode, struct file *file)
{
    printk("[%s] open", DRIVER_NAME);
    // GPIO Function Selectレジスタを設定
    // カーネルも仮想空間上で動くので物理アドレスを仮想アドレスに変換する
    int addr = (int)ioremap_nocache(REG_ADDR_GPIO_BASE + REG_ADDR_GPIO_GPFSEL_0, 15);
    // GPIO 4を出力に設定(12ビット目)
    set_register(addr, 1 << 12);
    // 仮想アドレスのマッピングを解除
    iounmap((void*)addr);

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
    // GPIO Pin Levelレジスタを参照してGPIOの入力を得る
    // 物理 -> 仮想アドレスマッピング
    int addr = (int)ioremap_nocache(REG_ADDR_GPIO_BASE + REG_ADDR_GPIO_LEVEL_0, 3);
    // Pin Levelレジスタの値を得る
    int reg_val = get_register(addr);
    // 3ビット目(GPIO3)の値を取り出し、1か0にする
    char val = (reg_val & (1 << 3)) > 0 ? '1' : '0';
    // 値をユーザーへ返す
    put_user(val + '\0', &buf[0]);
    // 仮想アドレスのマップを解除
    iounmap((void*)addr);
    return 1;
}

// デバイスwrite時の処理
static ssize_t hoge_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    printk("[%s] write", DRIVER_NAME);
    // GPIO Pin SetレジスタとGPIO Pin Clearレジスタを使って出力を行う
    // ユーザーからの入力を受け取る
    char userVal;
    get_user(userVal, &buf[0]);
    printk("%d", userVal);
    int addr = -1;
    // ユーザーの入力値によってHighにするかLowにするかを分岐
    if(userVal == '1') {
        // 1ならPin Setレジスタ
        addr = (int)ioremap_nocache(REG_ADDR_GPIO_BASE + REG_ADDR_GPIO_OUTPUT_SET_0, 5);
    } else if (userVal == '0') {
        // 0ならPin Clearレジスタ
        addr = (int)ioremap_nocache(REG_ADDR_GPIO_BASE + REG_ADDR_GPIO_OUTPUT_CLR_0, 5);
    }

    // 仮想アドレスを取得できたなら、レジスタに書き込む
    if (addr != -1) {
        // GPIO4なので4bit目
        set_register(addr, 1 << 4);
        iounmap((void*)addr);
        print_register(addr);
    }

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
