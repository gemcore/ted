#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(lfs_data);

static struct fs_mount_t lfs_mnt = {
    .type = FS_LITTLEFS,
    .fs_data = &lfs_data,
    .storage_dev = (void *)FIXED_PARTITION_ID(littlefs_storage),
    .mnt_point = "/lfs",
};

int main(void)
{
    int rc = fs_mount(&lfs_mnt);

    if (rc != 0) {
        printk("LittleFS mount failed: %d\n", rc);
    }
    return 0;
}
