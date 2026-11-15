#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>

struct hw_info_data {
	unsigned int mmc_manfid;
	char mmc_prod_name[8];
	unsigned int gsensor_idx;
};

static struct hw_info_data g_hw_info;
static unsigned int g_mem_idx;

static const char * const g_gsensor_names[] = {
	"STK8BA50-S",
	"SC7A20TR",
	"NULL",
};

static const char * const g_memory_names[] = {
	"KMFE60012M_B214",
	"16EMCP08_NL3DTB28",
	"KMFE60012A_B214",
	"NULL",
};

void hw_info_set_mmc(unsigned int manfid, const char *prod_name)
{
	g_hw_info.mmc_manfid = manfid;
	if (prod_name)
		strlcpy(g_hw_info.mmc_prod_name, prod_name,
			sizeof(g_hw_info.mmc_prod_name));
}
EXPORT_SYMBOL(hw_info_set_mmc);

void hw_info_set_gsensor(unsigned int idx)
{
	if (idx < ARRAY_SIZE(g_gsensor_names))
		g_hw_info.gsensor_idx = idx;
}
EXPORT_SYMBOL(hw_info_set_gsensor);

static ssize_t hw_info_write(struct file *file, const char __user *buf,
			     size_t count, loff_t *ppos)
{
	return count;
}

static int hw_info_show(struct seq_file *m, void *v)
{
	unsigned int g_idx = g_hw_info.gsensor_idx;

	if (g_hw_info.mmc_manfid == 0x45 &&
	    strcmp(g_hw_info.mmc_prod_name, "FMAEE8") == 0)
		g_mem_idx = 1;
	else if (g_hw_info.mmc_manfid == 0x32 &&
		 strcmp(g_hw_info.mmc_prod_name, "FE62AB") == 0)
		g_mem_idx = 2;
	else if (strncmp(g_hw_info.mmc_prod_name, "FE62MB", 6) == 0)
		g_mem_idx = 0;
	else
		g_mem_idx = 3;

	if (g_idx >= ARRAY_SIZE(g_gsensor_names))
		g_idx = 0;

	seq_printf(m,
		   "%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n",
		   "Camera_Bg", "5035",
		   "Camera_Fg", "5035",
		   "GSensor_IC", g_gsensor_names[g_idx],
		   "LCM_IC", "st7703",
		   "LCM_Vendor", "DEZHIXIN",
		   "Touch_IC", "FT5446U",
		   "Touch_Vendor", "HELITAI",
		   "Current_Momery", g_memory_names[g_mem_idx]);

	return 0;
}

static int hw_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, hw_info_show, NULL);
}

static const struct file_operations hw_info_fops = {
	.owner = THIS_MODULE,
	.open = hw_info_open,
	.read = seq_read,
	.write = hw_info_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init hw_info_init(void)
{
	struct proc_dir_entry *dir;

	dir = proc_mkdir("hw_info", NULL);
	proc_create_data("hw_info", 0644, dir, &hw_info_fops, NULL);
	return 0;
}
module_init(hw_info_init);

MODULE_LICENSE("GPL");
