#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/init.h>      // included for __init and __exit macros
#include <linux/fs.h>

#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/init.h>      // included for __init and __exit macros
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Adam Elhakham");
MODULE_DESCRIPTION("A driver for a remote char device");




// MODULE_LICENSE("GPL");
// MODULE_AUTHOR("Adam");
// MODULE_DESCRIPTION("A Simple Hello World module");

// static int __init hello_init(void)
// {
//     printk(KERN_INFO "Hello world!\n");
//     return 0;    // Non-zero return means that the module couldn't be loaded.
// }

// static void __exit hello_cleanup(void)
// {
//     printk(KERN_INFO "Cleaning up module.fed\n");
// }

// module_init(hello_init);
// module_exit(hello_cleanup);

/*
 * Sample disk driver, from the beginning.
 */

//#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/timer.h>
#include <linux/types.h>	/* size_t */
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/hdreg.h>	/* HDIO_GETGEO */
#include <linux/kdev_t.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>	/* invalidate_bdev */
#include <linux/bio.h>


static int sbull_major = 0;
//module_param(sbull_major, int, 0);
static int hardsect_size = 512;
//module_param(hardsect_size, int, 0);
static int nsectors = 1024;	/* How big the drive is */
//module_param(nsectors, int, 0);
static int ndevices = 1;
//module_param(ndevices, int, 0);

/*
 * The different "request modes" we can use.
 */
enum {
	RM_SIMPLE  = 0,	/* The extra-simple request function */
	RM_FULL    = 1,	/* The full-blown version */
	RM_NOQUEUE = 2,	/* Use make_request */
};
static int request_mode = RM_SIMPLE;
module_param(request_mode, int, 0);

/*
 * Minor number and partition management.
 */
#define SBULL_MINORS	16
#define MINOR_SHIFT	4
#define DEVNUM(kdevnum)	(MINOR(kdev_t_to_nr(kdevnum)) >> MINOR_SHIFT

/*
 * We can tweak our hardware sector size, but the kernel talks to us
 * in terms of small sectors, always.
 */
#define KERNEL_SECTOR_SIZE	512

/*
 * After this much idle time, the driver will simulate a media change.
 */
#define INVALIDATE_DELAY	30*HZ

/*
 * The internal representation of our device.
 */
struct sbull_dev {
        int size;                       /* Device size in sectors */
        u8 *data;                       /* The data array */
        short users;                    /* How many users */
        //short media_change;             /* Flag a media change? */
        spinlock_t lock;                /* For mutual exclusion */
        struct request_queue *queue;    /* The device request queue */
        struct gendisk *gd;             /* The gendisk structure */
        //struct timer_list timer;        /* For simulated media changes */
};

static struct sbull_dev *Devices = NULL;

/*
 * Handle an I/O request.
 */
static void sbull_transfer(struct sbull_dev *dev, unsigned long sector,
		unsigned long nsect, char *buffer, int write)
{
	unsigned long offset = sector*KERNEL_SECTOR_SIZE;
	unsigned long nbytes = nsect*KERNEL_SECTOR_SIZE;
    printk("rmtfile: sbull_transfer sector=%ld nsect=%ld buffer=%p write=%d nbytes=%ld-->\n", sector, nsect, buffer, write, nbytes);
	if ((offset + nbytes) > dev->size) {
		printk (KERN_NOTICE "Beyond-end write (%ld %ld)\n", offset, nbytes);
		return;
	}
	if (write) {
        printk("rmtfile: sbull_transfer write\n");
		memcpy(dev->data + offset, buffer, nbytes);
    }
	else {
        printk("rmtfile: sbull_transfer read\n");
		memcpy(buffer, dev->data + offset, nbytes);
    }
    printk("rmtfile: sbull_transfer <--\n");
}

/*
 * The simple form of the request function.
 */
static void sbull_request(struct request_queue *q)
{
	struct request *req;

    printk("rmtfile: sbull_request -->\n");
	while ((req = blk_fetch_request(q)) != NULL) {
		struct sbull_dev *dev = req->rq_disk->private_data;
        void *buffer = bio_data(req->bio);
        printk("rmtfile: got buffer %p-->\n", buffer);
        if (!buffer) {
            printk("rmtfile: before  blk_end_request_cur<--1\n");
            // blk_end_request_cur(req, -1);
            if (!__blk_end_request_cur(req, 0)) {
                printk("rmtfile: __blk_end_request_cur is null\n");
                break;
            }
            printk("rmtfile: after  blk_end_request_cur<--1\n");
            continue;
        }
		if (req->cmd_type != REQ_TYPE_FS) {
            printk("rmtfile: before  blk_end_request_cur<--2\n");
			printk (KERN_NOTICE "Skip non-fs request\n");
			//blk_end_request_cur(req, -1);
            if (!__blk_end_request_cur(req, 0)) {
                printk("rmtfile: __blk_end_request_cur is null\n");
                break;
            }
            printk("rmtfile: after  blk_end_request_cur<--2\n");
			continue;
		}
    //    	printk (KERN_NOTICE "Req dev %d dir %ld sec %ld, nr %d f %lx\n",
    //    			dev - Devices, rq_data_dir(req),
    //    			req->sector, req->current_nr_sectors,
    //    			req->flags);
        
		sbull_transfer(dev, blk_rq_pos(req), blk_rq_cur_sectors(req),
				buffer, rq_data_dir(req));
        printk("rmtfile: before  blk_end_request_cur<--3\n");
		// blk_end_request_cur(req, 0);
        if (!__blk_end_request_cur(req, 0)) {
            printk("rmtfile: __blk_end_request_cur is null\n");
            break;
        }
        printk("rmtfile: after  blk_end_request_cur<--3\n");
	}
    printk("rmtfile: sbull_request <--\n");
}


/*
 * Transfer a single BIO.
 */
// static int sbull_xfer_bio(struct sbull_dev *dev, struct bio *bio)
// {
// 	int i;
// 	struct bio_vec *bvec;
// 	sector_t sector = bio->bi_sector;

// 	/* Do each segment independently. */
// 	bio_for_each_segment(bvec, bio, i) {
// 		char *buffer = __bio_kmap_atomic(bio, i, KM_USER0);
// 		sbull_transfer(dev, sector, bio_cur_sectors(bio),
// 				buffer, bio_data_dir(bio) == WRITE);
// 		sector += bio_cur_sectors(bio);
// 		__bio_kunmap_atomic(bio, KM_USER0);
// 	}
// 	return 0; /* Always "succeed" */
// }

/*
 * Transfer a full request.
 */
// static int sbull_xfer_request(struct sbull_dev *dev, struct request *req)
// {
// 	struct bio *bio;
// 	int nsect = 0;
    
// 	rq_for_each_bio(bio, req) {
// 		sbull_xfer_bio(dev, bio);
// 		nsect += bio->bi_size/KERNEL_SECTOR_SIZE;
// 	}
// 	return nsect;
// }



/*
 * Smarter request function that "handles clustering".
 */
// static void sbull_full_request(struct request_queue *q)
// {
// 	struct request *req;
// 	int sectors_xferred;
// 	struct sbull_dev *dev = q->queuedata;

// 	while ((req = elv_next_request(q)) != NULL) {
// 		if (! blk_fs_request(req)) {
// 			printk (KERN_NOTICE "Skip non-fs request\n");
// 			end_request(req, 0);
// 			continue;
// 		}
// 		sectors_xferred = sbull_xfer_request(dev, req);
// 		if (! end_that_request_first(req, 1, sectors_xferred)) {
// 			blkdev_dequeue_request(req);
// 			end_that_request_last(req);
// 		}
// 	}
// }



/*
 * The direct make request version.
 */
// static int sbull_make_request(struct request_queue *q, struct bio *bio)
// {
// 	struct sbull_dev *dev = q->queuedata;
// 	int status;

// 	status = sbull_xfer_bio(dev, bio);
// 	bio_endio(bio, bio->bi_size, status);
// 	return 0;
// }


/*
 * Open and close.
 */

static int sbull_open(struct block_device *i_bdev, fmode_t mode)
{
	struct sbull_dev *dev = i_bdev->bd_disk->private_data;
    printk("rmtfile: sbull_open -->\n");
	//del_timer_sync(&dev->timer);
	// filp->private_data = dev;
	spin_lock(&dev->lock);
	// if (! dev->users) 
	// 	check_disk_change(i_bdev);
	dev->users++;
	spin_unlock(&dev->lock);
    printk("rmtfile: sbull_open <--\n");
	return 0;
}

static void sbull_release(struct gendisk *disk, fmode_t mode)
{
	struct sbull_dev *dev = disk->private_data;
    printk("rmtfile: sbull_release -->\n");
	spin_lock(&dev->lock);
	dev->users--;

	// if (!dev->users) {
	// 	dev->timer.expires = jiffies + INVALIDATE_DELAY;
	// 	add_timer(&dev->timer);
	// }
	spin_unlock(&dev->lock);
    printk("rmtfile: sbull_release <--\n");
}

/*
 * Look for a (simulated) media change.
 */
// static int sbull_media_changed(struct gendisk *gd)
// {
// 	struct sbull_dev *dev = gd->private_data;
	
// 	return dev->media_change;
// }

/*
 * Revalidate.  WE DO NOT TAKE THE LOCK HERE, for fear of deadlocking
 * with open.  That needs to be reevaluated.
 */
// static int sbull_revalidate(struct gendisk *gd)
// {
// 	struct sbull_dev *dev = gd->private_data;
	
// 	if (dev->media_change) {
// 		dev->media_change = 0;
// 		memset (dev->data, 0, dev->size);
// 	}
// 	return 0;
// }

/*
 * The "invalidate" function runs out of the device timer; it sets
 * a flag to simulate the removal of the media.
 */
// static void sbull_invalidate(unsigned long ldev)
// {
// 	struct sbull_dev *dev = (struct sbull_dev *) ldev;

// 	spin_lock(&dev->lock);
// 	if (dev->users || !dev->data) 
// 		printk (KERN_WARNING "sbull: timer sanity check failed\n");
// 	else
// 		dev->media_change = 1;
// 	spin_unlock(&dev->lock);
// }


/*
 * The ioctl() implementation
 */

static int sbull_ioctl (struct block_device *bd, fmode_t mode,
                 unsigned int cmd, unsigned long arg)
{
	long size;
	struct hd_geometry geo;
	struct sbull_dev *dev = bd->bd_disk->private_data;

    printk ("rmtfile: sbull_ioctl -->\n");
	switch(cmd) {
	    case HDIO_GETGEO:
        	/*
		 * Get geometry: since we are a virtual device, we have to make
		 * up something plausible.  So we claim 16 sectors, four heads,
		 * and calculate the corresponding number of cylinders.  We set the
		 * start of data at sector four.
		 */
        printk ("rmtfile: sbull_ioctl HDIO_GETGEO-->\n");
		size = dev->size*(hardsect_size/KERNEL_SECTOR_SIZE);
		geo.cylinders = (size & ~0x3f) >> 6;
		geo.heads = 4;
		geo.sectors = 16;
		geo.start = 4;
        printk ("rmtfile: before copy_to_user -->\n");
		if (copy_to_user((void __user *) arg, &geo, sizeof(geo))) {
            printk ("rmtfile: before copy_to_user failed-->\n");
			return -EFAULT;
        }
        printk ("rmtfile: sbull_ioctl -->\n");
		return 0;
	}

    printk ("rmtfile: sbull_ioctl bad command-->\n");
	return -ENOTTY; /* unknown command */
}



/*
 * The device operations structure.
 */
static struct block_device_operations sbull_ops = {
	.owner           = THIS_MODULE,
	.open 	         = sbull_open,
	.release 	     = sbull_release,
	// .media_changed   = sbull_media_changed,
	// .revalidate_disk = sbull_revalidate,
	.ioctl	         = sbull_ioctl
};

/*
 * Set up our internal device.
 */
static void setup_device(struct sbull_dev *dev, int which)
{
	/*
	 * Get some memory.
	 */
    printk ("rmtfile: setup_device -->\n");
	memset (dev, 0, sizeof (struct sbull_dev));
	dev->size = nsectors*hardsect_size;
	dev->data = vmalloc(dev->size);
    printk ("rmtfile: after vmalloc\n");
	if (dev->data == NULL) {
		printk (KERN_NOTICE "rmtfile: vmalloc failure.\n");
		return;
	}
	spin_lock_init(&dev->lock);

	// /*
	//  * The timer which "invalidates" the device.
	//  */
	// init_timer(&dev->timer);
	// dev->timer.data = (unsigned long) dev;
    // dev->timer.function = sbull_invalidate;
    printk ("rmtfile: call blk_init_queue\n");
    dev->queue = blk_init_queue(sbull_request, &dev->lock);
    printk ("after blk_init_queue\n");
    if (dev->queue == NULL)
        goto out_vfree;
    
    printk ("rmtfile: blk_init_queue successfull\n");
	/*
	 * The I/O queue, depending on whether we are using our own
	 * make_request function or not.
	 */
	// switch (request_mode) {
	    // case RM_NOQUEUE:
		// dev->queue = blk_alloc_queue(GFP_KERNEL);
		// if (dev->queue == NULL)
		// 	goto out_vfree;
		// blk_queue_make_request(dev->queue, sbull_make_request);
		// break;

	    //case RM_FULL:
		// dev->queue = blk_init_queue(sbull_full_request, &dev->lock);
		// if (dev->queue == NULL)
		// 	goto out_vfree;
		//break;

	    // default:
		// printk(KERN_NOTICE "Bad request mode %d, using simple\n", request_mode);
        	/* fall into.. */
	
	//     case RM_SIMPLE:
    //         dev->queue = blk_init_queue(sbull_request, &dev->lock);
    //         if (dev->queue == NULL)
    //             goto out_vfree;
    //         break;
	// }
	//blk_queue_hardsect_size(dev->queue, hardsect_size);
	dev->queue->queuedata = dev;
	/*
	 * And the gendisk structure.
	 */
    printk ("rmtfile: call alloc_disk\n");
	dev->gd = alloc_disk(SBULL_MINORS);
	if (! dev->gd) {
		printk (KERN_NOTICE "rmtfile: alloc_disk failure\n");
		goto out_vfree;
	}
    printk ("rmtfile: alloc_disk successful\n");
	dev->gd->major = sbull_major;
	dev->gd->first_minor = which*SBULL_MINORS;
	dev->gd->fops = &sbull_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;
    printk ("rmtfile: before snprintf\n");
	snprintf (dev->gd->disk_name, 32, "rmtfile%c", which + 'a');
    printk ("rmtfile: before set_capacity\n");
	set_capacity(dev->gd, nsectors*(hardsect_size/KERNEL_SECTOR_SIZE));
    printk ("rmtfile: after set_capacity\n");
	add_disk(dev->gd);
    printk ("rmtfile: after set_capacity\n");
	return;

  out_vfree:
    printk ("rmtfile: out_vfree\n");
	if (dev->data)
		vfree(dev->data);
}



static int __init hello_init(void){
	int i;
	/*
	 * Get registered.
	 */
	sbull_major = register_blkdev(sbull_major, "rmtfile");
	if (sbull_major <= 0) {
		printk(KERN_WARNING "rmtfile: unable to get major number\n");
		return -EBUSY;
	}
    printk("rmtfile: register major %d\n", sbull_major);
	/*
	 * Allocate the device array, and initialize each one.
	 */
	Devices = kmalloc(ndevices*sizeof (struct sbull_dev), GFP_KERNEL);
	if (Devices == NULL)
		goto out_unregister;
	for (i = 0; i < ndevices; i++) {
		setup_device(&Devices[i], i);
    }
    printk("rmtfile: register major %d SUCCESS!\n", sbull_major);
	return 0;

  out_unregister:
    printk("rmtfile: register major %d FAIL!\n", sbull_major);
	unregister_blkdev(sbull_major, "sbd");
	return -ENOMEM;
}

static void __exit hello_cleanup(void)
{
	int i;

	for (i = 0; i < ndevices; i++) {
		struct sbull_dev *dev = Devices + i;

		//del_timer_sync(&dev->timer);
		if (dev->gd) {
			del_gendisk(dev->gd);
			put_disk(dev->gd);
		}
		if (dev->queue) {
			// if (request_mode == RM_NOQUEUE)
			// 	blk_put_queue(dev->queue);
			// else
				blk_cleanup_queue(dev->queue);
		}
		if (dev->data)
			vfree(dev->data);
	}
    printk("rmtfile: unregister\n");
	unregister_blkdev(sbull_major, "rmtfile");
    printk("rmtfile: unregister success\n");
	kfree(Devices);
}
	
module_init(hello_init);
module_exit(hello_cleanup);
