
<!-- saved from url=(0094)http://git.tiamat-dev.com/8x60/htc-kernel-msm8x60/plain/arch/arm/mach-msm/board-holiday-wifi.c -->
<html><head><meta http-equiv="Content-Type" content="text/html; charset=UTF-8"></head><body><pre style="word-wrap: break-word; white-space: pre-wrap;">/* linux/arch/arm/mach-msm/board-holiday-wifi.c
*/
#include &lt;linux/kernel.h&gt;
#include &lt;linux/init.h&gt;
#include &lt;linux/platform_device.h&gt;
#include &lt;linux/delay.h&gt;
#include &lt;linux/err.h&gt;
#include &lt;asm/mach-types.h&gt;
#include &lt;asm/gpio.h&gt;
#include &lt;asm/io.h&gt;
#include &lt;linux/skbuff.h&gt;
#ifdef CONFIG_BCM4329_PURE_ANDROID
#include &lt;linux/wlan_plat.h&gt;
#else
#include &lt;linux/wifi_tiwlan.h&gt;
#endif

#include "board-holiday.h"

int holiday_wifi_power(int on);
int holiday_wifi_reset(int on);
int holiday_wifi_set_carddetect(int on);
int holiday_wifi_get_mac_addr(unsigned char *buf);

#define PREALLOC_WLAN_NUMBER_OF_SECTIONS	4
#define PREALLOC_WLAN_NUMBER_OF_BUFFERS		160
#define PREALLOC_WLAN_SECTION_HEADER		24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 1024)

#define WLAN_SKB_BUF_NUM	16

#define HW_OOB 1

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

typedef struct wifi_mem_prealloc_struct {
	void *mem_ptr;
	unsigned long size;
} wifi_mem_prealloc_t;

static wifi_mem_prealloc_t wifi_mem_array[PREALLOC_WLAN_NUMBER_OF_SECTIONS] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};

static void *holiday_wifi_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_NUMBER_OF_SECTIONS)
		return wlan_static_skb;
	if ((section &lt; 0) || (section &gt; PREALLOC_WLAN_NUMBER_OF_SECTIONS))
		return NULL;
	if (wifi_mem_array[section].size &lt; size)
		return NULL;
	return wifi_mem_array[section].mem_ptr;
}

int __init holiday_init_wifi_mem(void)
{
	int i;

	for (i = 0; (i &lt; WLAN_SKB_BUF_NUM); i++) {
		if (i &lt; (WLAN_SKB_BUF_NUM/2))
			wlan_static_skb[i] = dev_alloc_skb(4096);
		else
			wlan_static_skb[i] = dev_alloc_skb(8192);
	}
	for (i = 0; (i &lt; PREALLOC_WLAN_NUMBER_OF_SECTIONS); i++) {
		wifi_mem_array[i].mem_ptr = kmalloc(wifi_mem_array[i].size,
							GFP_KERNEL);
		if (wifi_mem_array[i].mem_ptr == NULL)
			return -ENOMEM;
	}
	return 0;
}

static struct resource holiday_wifi_resources[] = {
	[0] = {
		.name		= "bcm4329_wlan_irq",
		.start		= MSM_GPIO_TO_INT(HOLIDAY_GPIO_WIFI_IRQ),
		.end		= MSM_GPIO_TO_INT(HOLIDAY_GPIO_WIFI_IRQ),
#ifdef CONFIG_BCM4329_PURE_ANDROID
		.flags          = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE,
#else
		.flags          = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWEDGE,
#endif
	},
};

static struct wifi_platform_data holiday_wifi_control = {
	.set_power      = holiday_wifi_power,
	.set_reset      = holiday_wifi_reset,
	.set_carddetect = holiday_wifi_set_carddetect,
	.mem_prealloc   = holiday_wifi_mem_prealloc,
	.get_mac_addr	= holiday_wifi_get_mac_addr,
#ifndef CONFIG_BCM4329_PURE_ANDROID
	.dot11n_enable  = 1,
	.cscan_enable	= 1,
#endif
};

static struct platform_device holiday_wifi_device = {
	.name           = "bcm4329_wlan",
	.id             = 1,
	.num_resources  = ARRAY_SIZE(holiday_wifi_resources),
	.resource       = holiday_wifi_resources,
	.dev            = {
		.platform_data = &amp;holiday_wifi_control,
	},
};

extern unsigned char *get_wifi_nvs_ram(void);

static unsigned holiday_wifi_update_nvs(char *str)
{
#define NVS_LEN_OFFSET		0x0C
#define NVS_DATA_OFFSET		0x40
	unsigned char *ptr;
	unsigned len;

	if (!str)
		return -EINVAL;
	ptr = get_wifi_nvs_ram();
	/* Size in format LE assumed */
	memcpy(&amp;len, ptr + NVS_LEN_OFFSET, sizeof(len));

	/* the last bye in NVRAM is 0, trim it */
	if (ptr[NVS_DATA_OFFSET + len - 1] == 0)
		len -= 1;

	if (ptr[NVS_DATA_OFFSET + len -1] != '\n') {
		len += 1;
		ptr[NVS_DATA_OFFSET + len -1] = '\n';
	}

	strcpy(ptr + NVS_DATA_OFFSET + len, str);
	len += strlen(str);
	memcpy(ptr + NVS_LEN_OFFSET, &amp;len, sizeof(len));
	return 0;
}

#ifdef HW_OOB
static unsigned strip_nvs_param(char* param)
{
        unsigned char *nvs_data;

        unsigned param_len;
        int start_idx, end_idx;

        unsigned char *ptr;
        unsigned len;

        if (!param)
                return -EINVAL;
        ptr = get_wifi_nvs_ram();
        /* Size in format LE assumed */
        memcpy(&amp;len, ptr + NVS_LEN_OFFSET, sizeof(len));

        /* the last bye in NVRAM is 0, trim it */
        if (ptr[NVS_DATA_OFFSET + len -1] == 0)
                len -= 1;

        nvs_data = ptr + NVS_DATA_OFFSET;

        param_len = strlen(param);

        /* search param */
        for (start_idx = 0; start_idx &lt; len - param_len; start_idx++) {
                if (memcmp(&amp;nvs_data[start_idx], param, param_len) == 0) {
                        break;
                }
        }

        end_idx = 0;
        if (start_idx &lt; len - param_len) {
                /* search end-of-line */
                for (end_idx = start_idx + param_len; end_idx &lt; len; end_idx++) {
                        if (nvs_data[end_idx] == '\n' || nvs_data[end_idx] == 0) {
                                break;
                        }
                }
        }

        if (start_idx &lt; end_idx) {
                /* move the remain data forward */
                for (; end_idx + 1 &lt; len; start_idx++, end_idx++) {
                        nvs_data[start_idx] = nvs_data[end_idx+1];
                }
                len = len - (end_idx - start_idx + 1);
                memcpy(ptr + NVS_LEN_OFFSET, &amp;len, sizeof(len));
        }
        return 0;
}
#endif

#define WIFI_MAC_PARAM_STR     "macaddr="
#define WIFI_MAX_MAC_LEN       17 /* XX:XX:XX:XX:XX:XX */

static uint
get_mac_from_wifi_nvs_ram(char* buf, unsigned int buf_len)
{
	unsigned char *nvs_ptr;
	unsigned char *mac_ptr;
	uint len = 0;

	if (!buf || !buf_len) {
		return 0;
	}

	nvs_ptr = get_wifi_nvs_ram();
	if (nvs_ptr) {
		nvs_ptr += NVS_DATA_OFFSET;
	}

	mac_ptr = strstr(nvs_ptr, WIFI_MAC_PARAM_STR);
	if (mac_ptr) {
		mac_ptr += strlen(WIFI_MAC_PARAM_STR);

		/* skip holidaying space */
		while (mac_ptr[0] == ' ') {
			mac_ptr++;
		}

		/* locate end-of-line */
		len = 0;
		while (mac_ptr[len] != '\r' &amp;&amp; mac_ptr[len] != '\n' &amp;&amp;
			mac_ptr[len] != '\0') {
			len++;
		}

		if (len &gt; buf_len) {
			len = buf_len;
		}
		memcpy(buf, mac_ptr, len);
	}

	return len;
}

#define ETHER_ADDR_LEN 6
int holiday_wifi_get_mac_addr(unsigned char *buf)
{
	static u8 ether_mac_addr[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0xFF};
	char mac[WIFI_MAX_MAC_LEN];
	unsigned mac_len;
	unsigned int macpattern[ETHER_ADDR_LEN];
	int i;

	mac_len = get_mac_from_wifi_nvs_ram(mac, WIFI_MAX_MAC_LEN);
	if (mac_len &gt; 0) {
		//Mac address to pattern
		sscanf( mac, "%02x:%02x:%02x:%02x:%02x:%02x",
		&amp;macpattern[0], &amp;macpattern[1], &amp;macpattern[2],
		&amp;macpattern[3], &amp;macpattern[4], &amp;macpattern[5]
		);

		for(i = 0; i &lt; ETHER_ADDR_LEN; i++) {
			ether_mac_addr[i] = (u8)macpattern[i];
		}
	}

	memcpy(buf, ether_mac_addr, sizeof(ether_mac_addr));

	printk("holiday_wifi_get_mac_addr = %02x %02x %02x %02x %02x %02x \n",
		ether_mac_addr[0],ether_mac_addr[1],ether_mac_addr[2],ether_mac_addr[3],ether_mac_addr[4],ether_mac_addr[5]);

	return 0;
}

int __init holiday_wifi_init(void)
{
	int ret;

	printk(KERN_INFO "%s: start\n", __func__);
#ifdef HW_OOB
	strip_nvs_param("sd_oobonly");
#else
	holiday_wifi_update_nvs("sd_oobonly=1\n");
#endif
	holiday_wifi_update_nvs("btc_params80=0\n");
	holiday_wifi_update_nvs("btc_params6=30\n");
	holiday_init_wifi_mem();
	ret = platform_device_register(&amp;holiday_wifi_device);
	return ret;
}

</pre></body><link rel="stylesheet" type="text/css" href="data:text/css,"></html>