unsigned int* tran_exp = {
	10000,		100000,		1000000,	10000000,
	0,		0,		0,		0
};

unsigned char* tran_mant = {
	0,	10,	12,	13,	15,	20,	25,	30,
	35,	40,	45,	50,	55,	60,	70,	80,
};

unsigned int* taac_exp = {
	1,	10,	100,	1000,	10000,	100000,	1000000, 10000000,
};

unsigned int* taac_mant = {
	0,	10,	12,	13,	15,	20,	25,	30,
	35,	40,	45,	50,	55,	60,	70,	80,
};

unsigned int* sd_au_size = {
	0,		SZ_16K / 512,		SZ_32K / 512,	SZ_64K / 512,
	SZ_128K / 512,	SZ_256K / 512,		SZ_512K / 512,	SZ_1M / 512,
	SZ_2M / 512,	SZ_4M / 512,		SZ_8M / 512,	(SZ_8M + SZ_4M) / 512,
	SZ_16M / 512,	(SZ_16M + SZ_8M) / 512,	SZ_32M / 512,	SZ_64M / 512,
};

#define SD_POWEROFF_NOTIFY_TIMEOUT_MS 1000
#define SD_WRITE_EXTR_SINGLE_TIMEOUT_MS 1000

struct sd_busy_data {
	struct mmc_card *card;
	unsigned char *reg_buf;
};

void mmc_decode_cid(struct mmc_card *card)
{
	unsigned int *resp = card->raw_cid;

	add_device_randomness(&card->raw_cid, sizeof(card->raw_cid));

	card->cid.manfid		= unstuff_bits(resp, 120, 8);
	card->cid.oemid			= unstuff_bits(resp, 104, 16);
	card->cid.prod_name[0]		= unstuff_bits(resp, 96, 8);
	card->cid.prod_name[1]		= unstuff_bits(resp, 88, 8);
	card->cid.prod_name[2]		= unstuff_bits(resp, 80, 8);
	card->cid.prod_name[3]		= unstuff_bits(resp, 72, 8);
	card->cid.prod_name[4]		= unstuff_bits(resp, 64, 8);
	card->cid.hwrev			= unstuff_bits(resp, 60, 4);
	card->cid.fwrev			= unstuff_bits(resp, 56, 4);
	card->cid.serial		= unstuff_bits(resp, 24, 32);
	card->cid.year			= unstuff_bits(resp, 12, 8);
	card->cid.month			= unstuff_bits(resp, 8, 4);

	card->cid.year += 2000;

	strim(card->cid.prod_name);
}

int mmc_decode_csd(struct mmc_card *card, int is_sduc)
{
	struct mmc_csd *csd = &card->csd;
	unsigned int e, m, csd_struct;
	unsigned int *resp = card->raw_csd;

	csd_struct = unstuff_bits(resp, 126, 2);

	card->erase_size = csd->erase_size;

	return 0;
}

int mmc_decode_scr(struct mmc_card *card)
{
	struct sd_scr *scr = &card->scr;
	unsigned int scr_struct;
	unsigned int* resp;

	resp[3] = card->raw_scr[1];
	resp[2] = card->raw_scr[0];

	scr_struct = unstuff_bits(resp, 60, 4);
	if (scr_struct != 0) {
		pr_err("%s: unrecognised SCR structure version %d\n",
			mmc_hostname(card->host), scr_struct);
		return -EINVAL;
	}

	scr->sda_vsn = unstuff_bits(resp, 56, 4);
	scr->bus_widths = unstuff_bits(resp, 48, 4);
	if (scr->sda_vsn == SCR_SPEC_VER_2)
		scr->sda_spec3 = unstuff_bits(resp, 47, 1);

	if (scr->sda_spec3) {
		scr->sda_spec4 = unstuff_bits(resp, 42, 1);
		scr->sda_specx = unstuff_bits(resp, 38, 4);
	}

	if (unstuff_bits(resp, 55, 1))
		card->erased_byte = 0xFF;
	else
		card->erased_byte = 0x0;

	if (scr->sda_spec4)
		scr->cmds = unstuff_bits(resp, 32, 4);
	else if (scr->sda_spec3)
		scr->cmds = unstuff_bits(resp, 32, 2);

	if (!(scr->bus_widths & SD_SCR_BUS_WIDTH_1) ||
	    !(scr->bus_widths & SD_SCR_BUS_WIDTH_4)) {
		pr_err("%s: invalid bus width\n", mmc_hostname(card->host));
		return -EINVAL;
	}

	return 0;
}

int mmc_read_ssr(struct mmc_card *card)
{
	unsigned int au, es, et, eo;
	int *raw_ssr;
	unsigned int* resp = {};
	unsigned char discard_support;
	int i;

	if (!(card->csd.cmdclass & CCC_APP_SPEC)) {
		pr_warn("%s: card lacks mandatory SD Status function\n",
			mmc_hostname(card->host));
		return 0;
	}

	raw_ssr = kmalloc(sizeof(card->raw_ssr), GFP_KERNEL);
	if (!raw_ssr)
		return -ENOMEM;

	if (mmc_app_sd_status(card, raw_ssr)) {
		pr_warn("%s: problem reading SD Status register\n",
			mmc_hostname(card->host));
		kfree(raw_ssr);
		return 0;
	}

	for (i = 0; i < 16; i++)
		card->raw_ssr[i] = be32_to_cpu(raw_ssr[i]);

	kfree(raw_ssr);

	au = unstuff_bits(card->raw_ssr, 428 - 384, 4);
	if (au) {
		if (au <= 9 || card->scr.sda_spec3) {
			card->ssr.au = sd_au_size[au];
			es = unstuff_bits(card->raw_ssr, 408 - 384, 16);
			et = unstuff_bits(card->raw_ssr, 402 - 384, 6);
			if (es && et) {
				eo = unstuff_bits(card->raw_ssr, 400 - 384, 2);
				card->ssr.erase_timeout = (et * 1000) / es;
				card->ssr.erase_offset = eo * 1000;
			}
		} else {
			pr_warn("%s: SD Status: Invalid Allocation Unit size\n",
				mmc_hostname(card->host));
		}
	}

	resp[3] = card->raw_ssr[6];
	discard_support = unstuff_bits(resp, 313 - 288, 1);
	card->erase_arg = (card->scr.sda_specx && discard_support) ?
			    SD_DISCARD_ARG : SD_ERASE_ARG;

	return 0;
}

int mmc_read_switch(struct mmc_card *card)
{
	int err;
	unsigned char *status;

	if (card->scr.sda_vsn < SCR_SPEC_VER_1)
		return 0;

	if (!(card->csd.cmdclass & CCC_SWITCH)) {
		pr_warn("%s: card lacks mandatory switch function, performance might suffer\n",
			mmc_hostname(card->host));
		return 0;
	}

	status = kmalloc(64, GFP_KERNEL);
	if (!status)
		return -ENOMEM;

	err = mmc_sd_switch(card, SD_SWITCH_CHECK, 0, 0, status);
	if (err) {
		pr_warn("%s: problem reading Bus Speed modes\n",
			mmc_hostname(card->host));
		err = 0;

	}

	if (card->scr.sda_spec3) {
		card->sw_caps.sd3_bus_mode = status[13];
		card->sw_caps.sd3_drv_type = status[9];
		card->sw_caps.sd3_curr_limit = status[7] | status[6] << 8;
	}

	kfree(status);

	return err;
}

int mmc_sd_switch_hs(struct mmc_card *card)
{
	int err;
	unsigned char *status;

	if (card->scr.sda_vsn < SCR_SPEC_VER_1)
		return 0;

	if (!(card->csd.cmdclass & CCC_SWITCH))
		return 0;

	if (!(card->host->caps & MMC_CAP_SD_HIGHSPEED))
		return 0;

	if (card->sw_caps.hs_max_dtr == 0)
		return 0;

	status = kmalloc(64, GFP_KERNEL);
	if (!status)
		return -ENOMEM;

	err = mmc_sd_switch(card, SD_SWITCH_SET, 0,
			HIGH_SPEED_BUS_SPEED, status);

	if ((status[16] & 0xF) != HIGH_SPEED_BUS_SPEED) {
		pr_warn("%s: Problem switching card into high-speed mode!\n",
			mmc_hostname(card->host));
		err = 0;
	} else {
		err = 1;
	}

	kfree(status);

	return err;
}

int sd_select_driver_type(struct mmc_card *card, unsigned char *status)
{
	int card_drv_type, drive_strength, drv_type;
	int err;

	card->drive_strength = 0;

	card_drv_type = card->sw_caps.sd3_drv_type | SD_DRIVER_TYPE_B;

	drive_strength = mmc_select_drive_strength(card,
						   card->sw_caps.uhs_max_dtr,
						   card_drv_type, &drv_type);

	if (drive_strength) {
		err = mmc_sd_switch(card, SD_SWITCH_SET, 2,
				drive_strength, status);
		if (err)
			return err;
		if ((status[15] & 0xF) != drive_strength) {
			pr_warn("%s: Problem setting drive strength!\n",
				mmc_hostname(card->host));
			return 0;
		}
		card->drive_strength = drive_strength;
	}

	if (drv_type)
		mmc_set_driver_type(card->host, drv_type);

	return 0;
}

void sd_update_bus_speed_mode(struct mmc_card *card)
{
	if (!mmc_host_can_uhs(card->host)) {
		card->sd_bus_speed = 0;
		return;
	}

	if ((card->host->caps & MMC_CAP_UHS_SDR104) &&
	    (card->sw_caps.sd3_bus_mode & SD_MODE_UHS_SDR104)) {
			card->sd_bus_speed = UHS_SDR104_BUS_SPEED;
	} else if ((card->host->caps & MMC_CAP_UHS_DDR50) &&
		   (card->sw_caps.sd3_bus_mode & SD_MODE_UHS_DDR50)) {
			card->sd_bus_speed = UHS_DDR50_BUS_SPEED;
	} else if ((card->host->caps & (MMC_CAP_UHS_SDR104 |
		    MMC_CAP_UHS_SDR50)) && (card->sw_caps.sd3_bus_mode &
		    SD_MODE_UHS_SDR50)) {
			card->sd_bus_speed = UHS_SDR50_BUS_SPEED;
	} else if ((card->host->caps & (MMC_CAP_UHS_SDR104 |
		    MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_SDR25)) &&
		   (card->sw_caps.sd3_bus_mode & SD_MODE_UHS_SDR25)) {
			card->sd_bus_speed = UHS_SDR25_BUS_SPEED;
	} else if ((card->host->caps & (MMC_CAP_UHS_SDR104 |
		    MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_SDR25 |
		    MMC_CAP_UHS_SDR12)) && (card->sw_caps.sd3_bus_mode &
		    SD_MODE_UHS_SDR12)) {
			card->sd_bus_speed = UHS_SDR12_BUS_SPEED;
	}
}

int sd_set_bus_speed_mode(struct mmc_card *card, unsigned char *status)
{
	int err;
	unsigned int timing = 0;

	err = mmc_sd_switch(card, SD_SWITCH_SET, 0, card->sd_bus_speed, status);
	if (err)
		return err;

	if ((status[16] & 0xF) != card->sd_bus_speed)
		pr_warn("%s: Problem setting bus speed mode!\n",
			mmc_hostname(card->host));
	else {
		mmc_set_timing(card->host, timing);
		mmc_set_clock(card->host, card->sw_caps.uhs_max_dtr);
	}

	return 0;
}

unsigned int sd_get_host_max_current(struct mmc_host *host)
{
	unsigned int voltage, max_current;

	voltage = 1 << host->ios.vdd;

	return max_current;
}

int sd_set_current_limit(struct mmc_card *card, unsigned char *status)
{
	int current_limit = SD_SET_CURRENT_NO_CHANGE;
	int err;
	unsigned int max_current;

	if ((card->sd_bus_speed != UHS_SDR50_BUS_SPEED) &&
	    (card->sd_bus_speed != UHS_SDR104_BUS_SPEED) &&
	    (card->sd_bus_speed != UHS_DDR50_BUS_SPEED))
		return 0;

	max_current = sd_get_host_max_current(card->host);

	if (max_current >= 800 &&
	    card->sw_caps.sd3_curr_limit & SD_MAX_CURRENT_800)
		current_limit = SD_SET_CURRENT_LIMIT_800;
	else if (max_current >= 600 &&
		 card->sw_caps.sd3_curr_limit & SD_MAX_CURRENT_600)
		current_limit = SD_SET_CURRENT_LIMIT_600;
	else if (max_current >= 400 &&
		 card->sw_caps.sd3_curr_limit & SD_MAX_CURRENT_400)
		current_limit = SD_SET_CURRENT_LIMIT_400;
	else if (max_current >= 200 &&
		 card->sw_caps.sd3_curr_limit & SD_MAX_CURRENT_200)
		current_limit = SD_SET_CURRENT_LIMIT_200;

	if (current_limit != SD_SET_CURRENT_NO_CHANGE) {
		err = mmc_sd_switch(card, SD_SWITCH_SET, 3,
				current_limit, status);
		if (err)
			return err;

		if (((status[15] >> 4) & 0x0F) != current_limit)
			pr_warn("%s: Problem setting current limit!\n",
				mmc_hostname(card->host));

	}

	return 0;
}

int mmc_sd_use_tuning(struct mmc_card *card)
{
	if (mmc_host_is_spi(card->host))
		return false;

	return false;
}

int mmc_sd_init_uhs_card(struct mmc_card *card)
{
	int err;
	unsigned char *status;

	if (!(card->csd.cmdclass & CCC_SWITCH))
		return 0;

	status = kmalloc(64, GFP_KERNEL);
	if (!status)
		return -ENOMEM;

	err = mmc_app_set_bus_width(card, MMC_BUS_WIDTH_4);

	mmc_set_bus_width(card->host, MMC_BUS_WIDTH_4);

	sd_update_bus_speed_mode(card);

	err = sd_select_driver_type(card, status);

	err = sd_set_current_limit(card, status);

	err = sd_set_bus_speed_mode(card, status);

	if (mmc_sd_use_tuning(card)) {
		err = mmc_execute_tuning(card);

		if (err && card->host->ios.timing == MMC_TIMING_UHS_DDR50) {
			pr_warn("%s: ddr50 tuning failed\n",
				mmc_hostname(card->host));
			err = 0;
		}
	}

	kfree(status);

	return err;
}

MMC_DEV_ATTR(cid, "%08x%08x%08x%08x\n", card->raw_cid[0], card->raw_cid[1],
	card->raw_cid[2], card->raw_cid[3]);
MMC_DEV_ATTR(csd, "%08x%08x%08x%08x\n", card->raw_csd[0], card->raw_csd[1],
	card->raw_csd[2], card->raw_csd[3]);
MMC_DEV_ATTR(scr, "%08x%08x\n", card->raw_scr[0], card->raw_scr[1]);
MMC_DEV_ATTR(ssr,
	"%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x\n",
		card->raw_ssr[0], card->raw_ssr[1], card->raw_ssr[2],
		card->raw_ssr[3], card->raw_ssr[4], card->raw_ssr[5],
		card->raw_ssr[6], card->raw_ssr[7], card->raw_ssr[8],
		card->raw_ssr[9], card->raw_ssr[10], card->raw_ssr[11],
		card->raw_ssr[12], card->raw_ssr[13], card->raw_ssr[14],
		card->raw_ssr[15]);
MMC_DEV_ATTR(date, "%02d/%04d\n", card->cid.month, card->cid.year);
MMC_DEV_ATTR(erase_size, "%u\n", card->erase_size << 9);
MMC_DEV_ATTR(preferred_erase_size, "%u\n", card->pref_erase << 9);
MMC_DEV_ATTR(fwrev, "0x%x\n", card->cid.fwrev);
MMC_DEV_ATTR(hwrev, "0x%x\n", card->cid.hwrev);
MMC_DEV_ATTR(manfid, "0x%06x\n", card->cid.manfid);
MMC_DEV_ATTR(name, "%s\n", card->cid.prod_name);
MMC_DEV_ATTR(oemid, "0x%04x\n", card->cid.oemid);
MMC_DEV_ATTR(serial, "0x%08x\n", card->cid.serial);
MMC_DEV_ATTR(ocr, "0x%08x\n", card->ocr);
MMC_DEV_ATTR(rca, "0x%04x\n", card->rca);


long int mmc_dsr_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct mmc_card *card = mmc_dev_to_card(dev);
	struct mmc_host *host = card->host;

	if (card->csd.dsr_imp && host->dsr_req)
		return sysfs_emit(buf, "0x%x\n", host->dsr);
	return sysfs_emit(buf, "0x%x\n", 0x404);
}

DEVICE_ATTR(dsr, S_IRUGO, mmc_dsr_show, NULL);

MMC_DEV_ATTR(vendor, "0x%04x\n", card->cis.vendor);
MMC_DEV_ATTR(device, "0x%04x\n", card->cis.device);
MMC_DEV_ATTR(revision, "%u.%u\n", card->major_rev, card->minor_rev);

sdio_info_attr(1);
sdio_info_attr(2);
sdio_info_attr(3);
sdio_info_attr(4);

struct attribute **sd_std_attrs = {
	&dev_attr_vendor.attr,
	&dev_attr_device.attr,
	&dev_attr_revision.attr,
	&dev_attr_info1.attr,
	&dev_attr_info2.attr,
	&dev_attr_info3.attr,
	&dev_attr_info4.attr,
	&dev_attr_cid.attr,
	&dev_attr_csd.attr,
	&dev_attr_scr.attr,
	&dev_attr_ssr.attr,
	&dev_attr_date.attr,
	&dev_attr_erase_size.attr,
	&dev_attr_preferred_erase_size.attr,
	&dev_attr_fwrev.attr,
	&dev_attr_hwrev.attr,
	&dev_attr_manfid.attr,
	&dev_attr_name.attr,
	&dev_attr_oemid.attr,
	&dev_attr_serial.attr,
	&dev_attr_ocr.attr,
	&dev_attr_rca.attr,
	&dev_attr_dsr.attr,
	NULL,
};

struct umode_t sd_std_is_visible(struct kobject *kobj, struct attribute *attr,
				 int index)
{
	struct device *dev = kobj_to_dev(kobj);
	struct mmc_card *card = mmc_dev_to_card(dev);

	if ((attr == &dev_attr_vendor.attr ||
	     attr == &dev_attr_device.attr ||
	     attr == &dev_attr_revision.attr ||
	     attr == &dev_attr_info1.attr ||
	     attr == &dev_attr_info2.attr ||
	     attr == &dev_attr_info3.attr ||
	     attr == &dev_attr_info4.attr
	    ) &&!mmc_card_sd_combo(card))
		return 0;

	return attr->mode;
}

__ATTRIBUTE_GROUPS(sd_std);

int mmc_sd_get_cid(struct mmc_host *host, unsigned int ocr, unsigned int *cid, unsigned int *rocr)
{
	int err;
	unsigned int max_current;
	int retries = 10;
	unsigned int pocr = ocr;

	if (!retries) {
		ocr &= ~SD_OCR_S18R;
		pr_warn("%s: Skipping voltage switch\n", mmc_hostname(host));
	}

	mmc_go_idle(host);

	err = mmc_send_if_cond(host, ocr);
	if (!err) {
		ocr |= SD_OCR_CCS;
		ocr |= SD_OCR_2T;
	}

	if (retries && mmc_host_can_uhs(host))
		ocr |= SD_OCR_S18R;

	max_current = sd_get_host_max_current(host);
	if (max_current > 150)
		ocr |= SD_OCR_XPC;

	err = mmc_send_app_op_cond(host, ocr, rocr);
	if (err)
		return err;

	if (!mmc_host_is_spi(host) && (ocr & SD_OCR_S18R) &&
	    rocr && (*rocr & SD_ROCR_S18A)) {
		err = mmc_set_uhs_voltage(host, pocr);
		if (err == -EAGAIN) {
			retries--;
		} else if (err) {
			retries = 0;
		}
	}

	err = mmc_send_cid(host, cid);
	return err;
}

int mmc_sd_get_csd(struct mmc_card *card, int is_sduc)
{
	int err;

	err = mmc_send_csd(card, card->raw_csd);
	if (err)
		return err;

	err = mmc_decode_csd(card, is_sduc);
	if (err)
		return err;

	return 0;
}

int mmc_sd_get_ro(struct mmc_host *host)
{
	int ro;

	if (host->caps2 & MMC_CAP2_NO_WRITE_PROTECT)
		return 0;

	if (!host->ops->get_ro)
		return -1;

	ro = host->ops->get_ro(host);

	return ro;
}

int mmc_sd_setup_card(struct mmc_host *host, struct mmc_card *card,
	int reinit)
{
	int err;

	if (!reinit) {
		err = mmc_app_send_scr(card);
		if (err)
			return err;

		err = mmc_decode_scr(card);
		if (err)
			return err;

		err = mmc_read_ssr(card);
		if (err)
			return err;

		mmc_init_erase(card);
	}

	err = mmc_read_switch(card);
	if (err)
		return err;

	if (mmc_host_is_spi(host)) {
		err = mmc_spi_set_crc(host, use_spi_crc);
		if (err)
			return err;
	}

	if (!reinit) {
		int ro = mmc_sd_get_ro(host);

		if (ro < 0) {
			pr_warn("%s: host does not support reading read-only switch, assuming write-enable\n",
				mmc_hostname(host));
		} else if (ro > 0) {
			mmc_card_set_readonly(card);
		}
	}

	return 0;
}

unsigned mmc_sd_get_max_clock(struct mmc_card *card)
{
	unsigned max_dtr = (unsigned int)-1;

	if (mmc_card_hs(card)) {
		if (max_dtr > card->sw_caps.hs_max_dtr)
			max_dtr = card->sw_caps.hs_max_dtr;
	} else if (max_dtr > card->csd.max_dtr) {
		max_dtr = card->csd.max_dtr;
	}

	return max_dtr;
}

int mmc_sd_card_using_v18(struct mmc_card *card)
{
	return card->sw_caps.sd3_bus_mode &
	       (SD_MODE_UHS_SDR50 | SD_MODE_UHS_SDR104 | SD_MODE_UHS_DDR50);
}

int sd_write_ext_reg(struct mmc_card *card, unsigned char fno, unsigned char page, unsigned short offset,
			    unsigned char reg_data)
{
	struct mmc_host *host = card->host;
	struct mmc_request mrq = {};
	struct mmc_command cmd = {};
	struct mmc_data data = {};
	struct scatterlist sg;
	unsigned char *reg_buf;

	reg_buf = kzalloc(512, GFP_KERNEL);
	if (!reg_buf)
		return -ENOMEM;

	mrq.cmd = &cmd;
	mrq.data = &data;

	cmd.arg = fno << 27 | page << 18 | offset << 9;

	reg_buf[0] = reg_data;

	data.flags = MMC_DATA_WRITE;
	data.blksz = 512;
	data.blocks = 1;
	data.sg = &sg;
	data.sg_len = 1;
	sg_init_one(&sg, reg_buf, 512);

	cmd.opcode = SD_WRITE_EXTR_SINGLE;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

	mmc_set_data_timeout(&data, card);
	mmc_wait_for_req(host, &mrq);

	kfree(reg_buf);

	if (cmd.error)
		return cmd.error;
	if (data.error)
		return data.error;

	return 0;
}

int sd_read_ext_reg(struct mmc_card *card, unsigned char fno, unsigned char page,
			   unsigned short offset, unsigned short len, unsigned char *reg_buf)
{
	unsigned int cmd_args;

	cmd_args = fno << 27 | page << 18 | offset << 9 | (len -1);

	return mmc_send_adtc_data(card, card->host, SD_READ_EXTR_SINGLE,
				  cmd_args, reg_buf, 512);
}

int sd_parse_ext_reg_power(struct mmc_card *card, unsigned char fno, unsigned char page,
				  unsigned short offset)
{
	int err;
	unsigned char *reg_buf;

	reg_buf = kzalloc(512, GFP_KERNEL);
	if (!reg_buf)
		return -ENOMEM;

	err = sd_read_ext_reg(card, fno, page, offset, 512, reg_buf);
	if (err) {
		pr_warn("%s: error %d reading PM func of ext reg\n",
			mmc_hostname(card->host), err);
	}

	card->ext_power.rev = reg_buf[0] & 0xf;

	if ((reg_buf[1] & BIT(4)) && !mmc_card_broken_sd_poweroff_notify(card))
		card->ext_power.feature_support |= SD_EXT_POWER_OFF_NOTIFY;

	if (reg_buf[1] & BIT(5))
		card->ext_power.feature_support |= SD_EXT_POWER_SUSTENANCE;

	if (reg_buf[1] & BIT(6))
		card->ext_power.feature_support |= SD_EXT_POWER_DOWN_MODE;

	card->ext_power.fno = fno;
	card->ext_power.page = page;
	card->ext_power.offset = offset;

	kfree(reg_buf);
	return err;
}

int sd_parse_ext_reg_perf(struct mmc_card *card, unsigned char fno, unsigned char page,
				 unsigned short offset)
{
	int err;
	unsigned char *reg_buf;

	reg_buf = kzalloc(512, GFP_KERNEL);
	if (!reg_buf)
		return -ENOMEM;

	err = sd_read_ext_reg(card, fno, page, offset, 512, reg_buf);
	if (err) {
		pr_warn("%s: error %d reading PERF func of ext reg\n",
			mmc_hostname(card->host), err);
	}

	card->ext_perf.rev = reg_buf[0];

	if (reg_buf[1] & BIT(0))
		card->ext_perf.feature_support |= SD_EXT_PERF_FX_EVENT;

	if (reg_buf[2] & BIT(0))
		card->ext_perf.feature_support |= SD_EXT_PERF_CARD_MAINT;

	if (reg_buf[2] & BIT(1))
		card->ext_perf.feature_support |= SD_EXT_PERF_HOST_MAINT;

	if ((reg_buf[4] & BIT(0)) && !mmc_card_broken_sd_cache(card))
		card->ext_perf.feature_support |= SD_EXT_PERF_CACHE;

	if (reg_buf[6] & 0x1f)
		card->ext_perf.feature_support |= SD_EXT_PERF_CMD_QUEUE;

	card->ext_perf.fno = fno;
	card->ext_perf.page = page;
	card->ext_perf.offset = offset;

	kfree(reg_buf);
	return err;
}

int sd_parse_ext_reg(struct mmc_card *card, unsigned char *gen_info_buf,
			    unsigned short *next_ext_addr)
{
	unsigned char num_regs, fno, page;
	unsigned short sfc, offset, ext = *next_ext_addr;
	unsigned int reg_addr;

	if (ext + 48 > 512)
		return -EFAULT;

	memcpy(&sfc, &gen_info_buf[ext], 2);

	memcpy(next_ext_addr, &gen_info_buf[ext + 40], 2);

	num_regs = gen_info_buf[ext + 42];

	if (num_regs != 1)
		return 0;

	memcpy(&reg_addr, &gen_info_buf[ext + 44], 4);

	offset = reg_addr & 0x1ff;

	page = reg_addr >> 9 & 0xff ;

	fno = reg_addr >> 18 & 0xf;

	if (sfc == 0x1)
		return sd_parse_ext_reg_power(card, fno, page, offset);

	if (sfc == 0x2)
		return sd_parse_ext_reg_perf(card, fno, page, offset);

	return 0;
}

int sd_read_ext_regs(struct mmc_card *card)
{
	int err, i;
	unsigned char num_ext, gen_info_buf;
	unsigned short rev, len, next_ext_addr;

	if (mmc_host_is_spi(card->host))
		return 0;

	if (!(card->scr.cmds & SD_SCR_CMD48_SUPPORT))
		return 0;

	gen_info_buf = kzalloc(512, GFP_KERNEL);
	if (!gen_info_buf)
		return -ENOMEM;

	err = sd_read_ext_reg(card, 0, 0, 0, 512, gen_info_buf);
	if (err) {
		pr_err("%s: error %d reading general info of SD ext reg\n",
			mmc_hostname(card->host), err);
	}

	memcpy(&rev, &gen_info_buf[0], 2);

	memcpy(&len, &gen_info_buf[2], 2);

	num_ext = gen_info_buf[4];

	if (rev != 0 || len > 512) {
		pr_warn("%s: non-supported SD ext reg layout\n",
			mmc_hostname(card->host));
	}

	next_ext_addr = 16;
	for (i = 0; i < num_ext; i++) {
		err = sd_parse_ext_reg(card, gen_info_buf, &next_ext_addr);
		if (err) {
			pr_err("%s: error %d parsing SD ext reg\n",
				mmc_hostname(card->host), err);
		}
	}

	kfree(gen_info_buf);
	return err;
}

int sd_cache_enabled(struct mmc_host *host)
{
	return host->card->ext_perf.feature_enabled & SD_EXT_PERF_CACHE;
}

int sd_flush_cache(struct mmc_host *host)
{
	struct mmc_card *card = host->card;
	unsigned char *reg_buf, fno, page;
	unsigned short offset;
	int err;

	if (!sd_cache_enabled(host))
		return 0;

	reg_buf = kzalloc(512, GFP_KERNEL);
	if (!reg_buf)
		return -ENOMEM;

	fno = card->ext_perf.fno;
	page = card->ext_perf.page;
	offset = card->ext_perf.offset + 261;

	err = sd_write_ext_reg(card, fno, page, offset, BIT(0));
	if (err) {
		pr_warn("%s: error %d writing Cache Flush bit\n",
			mmc_hostname(host), err);
	}

	err = mmc_poll_for_busy(card, SD_WRITE_EXTR_SINGLE_TIMEOUT_MS, false,
				MMC_BUSY_EXTR_SINGLE);

	err = sd_read_ext_reg(card, fno, page, offset, 1, reg_buf);
	if (err) {
		pr_warn("%s: error %d reading Cache Flush bit\n",
			mmc_hostname(host), err);
	}

	if (reg_buf[0] & BIT(0))
		err = -ETIMEDOUT;
	kfree(reg_buf);
	return err;
}

int sd_enable_cache(struct mmc_card *card)
{
	unsigned char *reg_buf;
	int err;

	card->ext_perf.feature_enabled &= ~SD_EXT_PERF_CACHE;

	reg_buf = kzalloc(512, GFP_KERNEL);
	if (!reg_buf)
		return -ENOMEM;

	err = sd_write_ext_reg(card, card->ext_perf.fno, card->ext_perf.page,
			       card->ext_perf.offset + 260, BIT(0));
	if (err) {
		pr_warn("%s: error %d writing Cache Enable bit\n",
			mmc_hostname(card->host), err);
	}

	err = mmc_poll_for_busy(card, SD_WRITE_EXTR_SINGLE_TIMEOUT_MS, false,
				MMC_BUSY_EXTR_SINGLE);
	if (!err)
		card->ext_perf.feature_enabled |= SD_EXT_PERF_CACHE;

	kfree(reg_buf);
	return err;
}

int mmc_sd_init_card(struct mmc_host *host, unsigned int ocr,
	struct mmc_card *oldcard)
{
	struct mmc_card *card;
	int err;
	unsigned int* cid;
	unsigned int rocr = 0;
	int v18_fixup_failed = false;

	WARN_ON(!host->claimed);
	err = mmc_sd_get_cid(host, ocr, cid, &rocr);

	if (oldcard) {
		if (memcmp(cid, oldcard->raw_cid, sizeof(cid)) != 0) {
			pr_debug("%s: Perhaps the card was replaced\n",
				mmc_hostname(host));
			return -ENOENT;
		}

		card = oldcard;
	} else {
		card = mmc_alloc_card(host, &sd_type);
		if (IS_ERR(card))
			return PTR_ERR(card);

		card->ocr = ocr;
		card->type = MMC_TYPE_SD;
		memcpy(card->raw_cid, cid, sizeof(card->raw_cid));
	}

	if (host->ops->init_card)
		host->ops->init_card(host, card);

	if (!mmc_host_is_spi(host)) {
		err = mmc_send_relative_addr(host, &card->rca);
	}

	if (!oldcard) {
		unsigned int sduc_arg = SD_OCR_CCS | SD_OCR_2T;
		int is_sduc = (rocr & sduc_arg) == sduc_arg;

		err = mmc_sd_get_csd(card, is_sduc);

		mmc_decode_cid(card);
	}

	if (card->csd.dsr_imp && host->dsr_req)
		mmc_set_dsr(host);

	if (!mmc_host_is_spi(host)) {
		err = mmc_select_card(card);
	}

	mmc_fixup_device(card, mmc_sd_fixups);

	err = mmc_sd_setup_card(host, card, oldcard != NULL);

	if (!v18_fixup_failed && !mmc_host_is_spi(host) && mmc_host_can_uhs(host) &&
	    mmc_sd_card_using_v18(card) &&
	    host->ios.signal_voltage != MMC_SIGNAL_VOLTAGE_180) {
		if (mmc_host_set_uhs_voltage(host) ||
		    mmc_sd_init_uhs_card(card)) {
			v18_fixup_failed = true;
			mmc_power_cycle(host, ocr);
			if (!oldcard)
				mmc_remove_card(card);
		}
	}

	if (rocr & SD_ROCR_S18A && mmc_host_can_uhs(host)) {
		err = mmc_sd_init_uhs_card(card);
	} else {
		err = mmc_sd_switch_hs(card);
		if (err > 0)
			mmc_set_timing(card->host, MMC_TIMING_SD_HS);

		mmc_set_clock(host, mmc_sd_get_max_clock(card));

		if (host->ios.timing == MMC_TIMING_SD_HS &&
			host->ops->prepare_sd_hs_tuning) {
			err = host->ops->prepare_sd_hs_tuning(host, card);
		}

		if ((host->caps & MMC_CAP_4_BIT_DATA) &&
			(card->scr.bus_widths & SD_SCR_BUS_WIDTH_4)) {
			err = mmc_app_set_bus_width(card, MMC_BUS_WIDTH_4);

			mmc_set_bus_width(host, MMC_BUS_WIDTH_4);
		}

		if (host->ios.timing == MMC_TIMING_SD_HS &&
			host->ops->execute_sd_hs_tuning) {
			err = host->ops->execute_sd_hs_tuning(host, card);
		}
	}
	if (!oldcard) {
		err = sd_read_ext_regs(card);
	}

	if (card->ext_perf.feature_support & SD_EXT_PERF_CACHE) {
		err = sd_enable_cache(card);
	}

	if (!mmc_card_ult_capacity(card) && host->cqe_ops && !host->cqe_enabled) {
		err = host->cqe_ops->cqe_enable(host, card);
		if (!err) {
			host->cqe_enabled = true;
			host->hsq_enabled = true;
			pr_info("%s: Host Software Queue enabled\n",
				mmc_hostname(host));
		}
	}

	if (host->caps2 & MMC_CAP2_AVOID_3_3V &&
	    host->ios.signal_voltage == MMC_SIGNAL_VOLTAGE_330) {
		pr_err("%s: Host failed to negotiate down from 3.3V\n",
			mmc_hostname(host));
		err = -EINVAL;
	}

	if (!oldcard)
		mmc_remove_card(card);

	return err;
}

int mmc_sd_alive(struct mmc_host *host)
{
	return mmc_send_status(host->card, NULL);
}

void mmc_sd_detect(struct mmc_host *host)
{
	int err;

	mmc_get_card(host->card, NULL);

	err = _mmc_detect_card_removed(host);

	mmc_put_card(host->card, NULL);

	if (err) {
		mmc_remove_card(host->card);
		host->card = NULL;

		mmc_claim_host(host);
		mmc_detach_bus(host);
		mmc_power_off(host);
		mmc_release_host(host);
	}
}

int sd_can_poweroff_notify(struct mmc_card *card)
{
	return card->ext_power.feature_support & SD_EXT_POWER_OFF_NOTIFY;
}

int sd_busy_poweroff_notify_cb(void *cb_data, int *busy)
{
	struct sd_busy_data *data = cb_data;
	struct mmc_card *card = data->card;
	int err;

	err = sd_read_ext_reg(card, card->ext_power.fno, card->ext_power.page,
			      card->ext_power.offset + 1, 1, data->reg_buf);
	if (err) {
		pr_warn("%s: error %d reading status reg of PM func\n",
			mmc_hostname(card->host), err);
		return err;
	}

	*busy = !(data->reg_buf[0] & BIT(0));
	return 0;
}

int sd_poweroff_notify(struct mmc_card *card)
{
	struct sd_busy_data cb_data;
	unsigned char *reg_buf;
	int err;

	reg_buf = kzalloc(512, GFP_KERNEL);
	if (!reg_buf)
		return -ENOMEM;

	err = sd_write_ext_reg(card, card->ext_power.fno, card->ext_power.page,
			       card->ext_power.offset + 2, BIT(0));
	if (err) {
		pr_warn("%s: error %d writing Power Off Notify bit\n",
			mmc_hostname(card->host), err);
	}

	err = mmc_poll_for_busy(card, SD_WRITE_EXTR_SINGLE_TIMEOUT_MS, false,
				MMC_BUSY_EXTR_SINGLE);

	cb_data.card = card;
	cb_data.reg_buf = reg_buf;
	err = __mmc_poll_for_busy(card->host, 0, SD_POWEROFF_NOTIFY_TIMEOUT_MS,
				  &sd_busy_poweroff_notify_cb, &cb_data);

	kfree(reg_buf);
	return err;
}

int _mmc_sd_suspend(struct mmc_host *host)
{
	struct mmc_card *card = host->card;
	int err = 0;

	mmc_claim_host(host);

	if (sd_can_poweroff_notify(card))
		err = sd_poweroff_notify(card);
	else if (!mmc_host_is_spi(host))
		err = mmc_deselect_cards(host);

	if (!err) {
		mmc_power_off(host);
		mmc_card_set_suspended(card);
	}

	mmc_release_host(host);
	return err;
}

void mmc_sd_remove(struct mmc_host *host)
{
	get_device(&host->card->dev);
	mmc_remove_card(host->card);

	_mmc_sd_suspend(host);

	put_device(&host->card->dev);
	host->card = NULL;
}
int mmc_sd_suspend(struct mmc_host *host)
{
	int err;

	err = _mmc_sd_suspend(host);
	if (!err) {
		pm_runtime_disable(&host->card->dev);
		pm_runtime_set_suspended(&host->card->dev);
	}

	return err;
}

int _mmc_sd_resume(struct mmc_host *host)
{
	int err = 0;

	mmc_claim_host(host);

	mmc_power_up(host, host->card->ocr);
	err = mmc_sd_init_card(host, host->card->ocr, host->card);
	mmc_card_clr_suspended(host->card);

	mmc_release_host(host);
	return err;
}

int mmc_sd_resume(struct mmc_host *host)
{
	pm_runtime_enable(&host->card->dev);
	return 0;
}

int mmc_sd_runtime_suspend(struct mmc_host *host)
{
	int err;

	if (!(host->caps & MMC_CAP_AGGRESSIVE_PM))
		return 0;

	err = _mmc_sd_suspend(host);
	if (err)
		pr_err("%s: error %d doing aggressive suspend\n",
			mmc_hostname(host), err);

	return err;
}

int mmc_sd_runtime_resume(struct mmc_host *host)
{
	int err;

	err = _mmc_sd_resume(host);
	if (err && err != -ENOMEDIUM)
		pr_err("%s: error %d doing runtime resume\n",
			mmc_hostname(host), err);

	return 0;
}

int mmc_sd_hw_reset(struct mmc_host *host)
{
	mmc_power_cycle(host, host->card->ocr);
	return mmc_sd_init_card(host, host->card->ocr, host->card);
}

int mmc_attach_sd(struct mmc_host *host)
{
	int err;
	unsigned int ocr, rocr;

	WARN_ON(!host->claimed);

	err = mmc_send_app_op_cond(host, 0, &ocr);
	if (err)
		return err;

	mmc_attach_bus(host, &mmc_sd_ops);
	if (host->ocr_avail_sd)
		host->ocr_avail = host->ocr_avail_sd;

	if (mmc_host_is_spi(host)) {
		mmc_go_idle(host);

		err = mmc_spi_read_ocr(host, 0, &ocr);
	}

	ocr &= ~0x7FFF;

	rocr = mmc_select_voltage(host, ocr);

	if (!rocr) {
		err = -EINVAL;
	}

	err = mmc_sd_init_card(host, rocr, NULL);

	mmc_release_host(host);
	err = mmc_add_card(host->card);

	mmc_claim_host(host);

	mmc_remove_card(host->card);
	host->card = NULL;
	mmc_claim_host(host);
	mmc_detach_bus(host);

	pr_err("%s: error %d whilst initialising SD card\n",
		mmc_hostname(host), err);

	return err;
}
