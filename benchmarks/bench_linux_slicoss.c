struct pci_device_id* slic_id_tbl = {
	{ PCI_DEVICE(PCI_VENDOR_ID_ALACRITECH,
		     PCI_DEVICE_ID_ALACRITECH_MOJAVE) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ALACRITECH,
		     PCI_DEVICE_ID_ALACRITECH_OASIS) },
	{ 0 }
};

char **slic_stats_strings = {
	"rx_packets",
	"rx_bytes",
	"rx_multicasts",
	"rx_errors",
	"rx_buff_miss",
	"rx_tp_csum",
	"rx_tp_oflow",
	"rx_tp_hlen",
	"rx_ip_csum",
	"rx_ip_len",
	"rx_ip_hdr_len",
	"rx_early",
	"rx_buff_oflow",
	"rx_lcode",
	"rx_drbl",
	"rx_crc",
	"rx_oflow_802",
	"rx_uflow_802",
	"tx_packets",
	"tx_bytes",
	"tx_carrier",
	"tx_dropped",
	"irq_errs",
};

int slic_next_queue_idx(unsigned int idx, unsigned int qlen)
{
	return (idx + 1) & (qlen - 1);
}

int slic_get_free_queue_descs(unsigned int put_idx,
					    unsigned int done_idx,
					    unsigned int qlen)
{
	if (put_idx >= done_idx)
		return (qlen - (put_idx - done_idx) - 1);
	return (done_idx - put_idx - 1);
}

unsigned int slic_next_compl_idx(struct slic_device *sdev)
{
	struct slic_stat_queue *stq = &sdev->stq;
	unsigned int active = stq->active_array;
	struct slic_stat_desc *descs;
	struct slic_stat_desc *stat;
	unsigned int idx;

	descs = stq->descs[active];
	stat = &descs[stq->done_idx];

	if (!stat->status)
		return SLIC_INVALID_STAT_DESC_IDX;

	idx = (le32_to_cpu(stat->hnd) & 0xffff) - 1;
	stat->hnd = 0;
	stat->status = 0;

	stq->done_idx = slic_next_queue_idx(stq->done_idx, stq->len);
	if (!stq->done_idx) {
		struct dma_addr_t paddr = stq->paddr[active];

		slic_write(sdev, SLIC_REG_RBAR, lower_32_bits(paddr) |
						stq->len);
		slic_flush_write(sdev);
		active++;
		active &= (SLIC_NUM_STAT_DESC_ARRAYS - 1);
		stq->active_array = active;
	}
	return idx;
}

unsigned int slic_get_free_tx_descs(struct slic_tx_queue *txq)
{
	smp_mb();
	return slic_get_free_queue_descs(txq->put_idx, txq->done_idx, txq->len);
}

unsigned int slic_get_free_rx_descs(struct slic_rx_queue *rxq)
{
	return slic_get_free_queue_descs(rxq->put_idx, rxq->done_idx, rxq->len);
}

void slic_clear_upr_list(struct slic_upr_list *upr_list)
{
	struct slic_upr *upr;
	struct slic_upr *tmp;

	spin_lock_bh(&upr_list->lock);
	upr_list->pending = false;
	spin_unlock_bh(&upr_list->lock);
}

void slic_start_upr(struct slic_device *sdev, struct slic_upr *upr)
{
	unsigned reg;

	reg = (upr->type == SLIC_UPR_CONFIG) ? SLIC_REG_RCONFIG :
					       SLIC_REG_LSTAT;
	slic_write(sdev, reg, lower_32_bits(upr->paddr));
	slic_flush_write(sdev);
}

void slic_queue_upr(struct slic_device *sdev, struct slic_upr *upr)
{
	struct slic_upr_list *upr_list = &sdev->upr_list;
	int pending;

	spin_lock_bh(&upr_list->lock);
	pending = upr_list->pending;
	INIT_LIST_HEAD(&upr->list);
	list_add_tail(&upr->list, &upr_list->list);
	upr_list->pending = true;
	spin_unlock_bh(&upr_list->lock);

	if (!pending)
		slic_start_upr(sdev, upr);
}

struct slic_upr *slic_dequeue_upr(struct slic_device *sdev)
{
	struct slic_upr_list *upr_list = &sdev->upr_list;
	struct slic_upr *next_upr = NULL;
	struct slic_upr *upr = NULL;

	spin_lock_bh(&upr_list->lock);
	if (!list_empty(&upr_list->list)) {
		upr = list_first_entry(&upr_list->list, struct slic_upr, list);
		list_del(&upr->list);

		if (list_empty(&upr_list->list))
			upr_list->pending = false;
		else
			next_upr = list_first_entry(&upr_list->list,
						    struct slic_upr, list);
	}
	spin_unlock_bh(&upr_list->lock);
	if (next_upr)
		slic_start_upr(sdev, next_upr);

	return upr;
}

int slic_new_upr(struct slic_device *sdev, unsigned int type,
			struct dma_addr_t paddr)
{
	struct slic_upr *upr;

	upr = kmalloc(sizeof(*upr), GFP_ATOMIC);
	if (!upr)
		return -ENOMEM;
	upr->type = type;
	upr->paddr = paddr;

	slic_queue_upr(sdev, upr);

	return 0;
}

void slic_set_mcast_bit(unsigned long int *mcmask, unsigned char *addr)
{
	unsigned long int mask = *mcmask;
	unsigned char crc;
	crc = ether_crc(ETH_ALEN, addr) >> 23;
	crc &= 0x3F;
	mask |= (unsigned long int)1 << crc;
	*mcmask = mask;
}

void slic_configure_rcv(struct slic_device *sdev)
{
	unsigned val;

	val = SLIC_GRCR_RESET | SLIC_GRCR_ADDRAEN | SLIC_GRCR_RCVEN |
	      SLIC_GRCR_HASHSIZE << SLIC_GRCR_HASHSIZE_SHIFT | SLIC_GRCR_RCVBAD;

	if (sdev->duplex == DUPLEX_FULL)
		val |= SLIC_GRCR_CTLEN;

	if (sdev->promisc)
		val |= SLIC_GRCR_RCVALL;

	slic_write(sdev, SLIC_REG_WRCFG, val);
}

void slic_configure_xmt(struct slic_device *sdev)
{
	unsigned val;

	val = SLIC_GXCR_RESET | SLIC_GXCR_XMTEN;

	if (sdev->duplex == DUPLEX_FULL)
		val |= SLIC_GXCR_PAUSEEN;

	slic_write(sdev, SLIC_REG_WXCFG, val);
}

void slic_configure_mac(struct slic_device *sdev)
{
	unsigned val;

	if (sdev->speed == SPEED_1000) {
		val = SLIC_GMCR_GAPBB_1000 << SLIC_GMCR_GAPBB_SHIFT |
		      SLIC_GMCR_GAPR1_1000 << SLIC_GMCR_GAPR1_SHIFT |
		      SLIC_GMCR_GAPR2_1000 << SLIC_GMCR_GAPR2_SHIFT |
		      SLIC_GMCR_GBIT;
	} else {
		val = SLIC_GMCR_GAPBB_100 << SLIC_GMCR_GAPBB_SHIFT |
		      SLIC_GMCR_GAPR1_100 << SLIC_GMCR_GAPR1_SHIFT |
		      SLIC_GMCR_GAPR2_100 << SLIC_GMCR_GAPR2_SHIFT;
	}

	if (sdev->duplex == DUPLEX_FULL)
		val |= SLIC_GMCR_FULLD;

	slic_write(sdev, SLIC_REG_WMCFG, val);
}

void slic_configure_link_locked(struct slic_device *sdev, int speed,
				       unsigned int duplex)
{
	struct net_device *dev = sdev->netdev;

	if (sdev->speed == speed && sdev->duplex == duplex)
		return;

	sdev->speed = speed;
	sdev->duplex = duplex;

	if (sdev->speed == SPEED_UNKNOWN) {
		if (netif_carrier_ok(dev))
			netif_carrier_off(dev);
	} else {
		slic_configure_mac(sdev);
		slic_configure_xmt(sdev);
		slic_configure_rcv(sdev);
		slic_flush_write(sdev);

		if (!netif_carrier_ok(dev))
			netif_carrier_on(dev);
	}
}

void slic_configure_link(struct slic_device *sdev, int speed,
				unsigned int duplex)
{
	spin_lock_bh(&sdev->link_lock);
	slic_configure_link_locked(sdev, speed, duplex);
	spin_unlock_bh(&sdev->link_lock);
}

void slic_set_rx_mode(struct net_device *dev)
{
	struct slic_device *sdev = netdev_priv(dev);
	struct netdev_hw_addr *hwaddr;
	int set_promisc;
	unsigned long int mcmask;

	if (dev->flags & (IFF_PROMISC | IFF_ALLMULTI)) {
		mcmask = ~(unsigned long int)0;
	} else 
		mcmask = 0;

	slic_write(sdev, SLIC_REG_MCASTLOW, lower_32_bits(mcmask));
	slic_write(sdev, SLIC_REG_MCASTHIGH, upper_32_bits(mcmask));

	set_promisc = !!(dev->flags & IFF_PROMISC);

	spin_lock_bh(&sdev->link_lock);
	if (sdev->promisc != set_promisc) {
		sdev->promisc = set_promisc;
		slic_configure_rcv(sdev);
	}
	spin_unlock_bh(&sdev->link_lock);
}


void slic_handle_frame_error(struct slic_device *sdev,
				    struct sk_buff *skb)
{
	struct slic_stats *stats = &sdev->stats;

	if (sdev->model == SLIC_MODEL_OASIS) {
		struct slic_rx_info_oasis *info;
		unsigned status_b;
		unsigned status;

		info = (struct slic_rx_info_oasis *)skb->data;
		status = le32_to_cpu(info->frame_status);
		status_b = le32_to_cpu(info->frame_status_b);
		if (status_b & SLIC_VRHSTATB_TPCSUM)
			SLIC_INC_STATS_COUNTER(stats, rx_tpcsum);
		if (status & SLIC_VRHSTAT_TPOFLO)
			SLIC_INC_STATS_COUNTER(stats, rx_tpoflow);
		if (status_b & SLIC_VRHSTATB_TPHLEN)
			SLIC_INC_STATS_COUNTER(stats, rx_tphlen);
		if (status_b & SLIC_VRHSTATB_IPCSUM)
			SLIC_INC_STATS_COUNTER(stats, rx_ipcsum);
		if (status_b & SLIC_VRHSTATB_IPLERR)
			SLIC_INC_STATS_COUNTER(stats, rx_iplen);
		if (status_b & SLIC_VRHSTATB_IPHERR)
			SLIC_INC_STATS_COUNTER(stats, rx_iphlen);
		if (status_b & SLIC_VRHSTATB_RCVE)
			SLIC_INC_STATS_COUNTER(stats, rx_early);
		if (status_b & SLIC_VRHSTATB_BUFF)
			SLIC_INC_STATS_COUNTER(stats, rx_buffoflow);
		if (status_b & SLIC_VRHSTATB_CODE)
			SLIC_INC_STATS_COUNTER(stats, rx_lcode);
		if (status_b & SLIC_VRHSTATB_DRBL)
			SLIC_INC_STATS_COUNTER(stats, rx_drbl);
		if (status_b & SLIC_VRHSTATB_CRC)
			SLIC_INC_STATS_COUNTER(stats, rx_crc);
		if (status & SLIC_VRHSTAT_802OE)
			SLIC_INC_STATS_COUNTER(stats, rx_oflow802);
		if (status_b & SLIC_VRHSTATB_802UE)
			SLIC_INC_STATS_COUNTER(stats, rx_uflow802);
		if (status_b & SLIC_VRHSTATB_CARRE)
			SLIC_INC_STATS_COUNTER(stats, tx_carrier);
	} else {
		struct slic_rx_info_mojave *info;
		unsigned status;

		info = (struct slic_rx_info_mojave *)skb->data;
		status = le32_to_cpu(info->frame_status);
		if (status & SLIC_VGBSTAT_XPERR) {
			unsigned xerr = status >> SLIC_VGBSTAT_XERRSHFT;

			if (xerr == SLIC_VGBSTAT_XCSERR)
				SLIC_INC_STATS_COUNTER(stats, rx_tpcsum);
			if (xerr == SLIC_VGBSTAT_XUFLOW)
				SLIC_INC_STATS_COUNTER(stats, rx_tpoflow);
			if (xerr == SLIC_VGBSTAT_XHLEN)
				SLIC_INC_STATS_COUNTER(stats, rx_tphlen);
		}
		if (status & SLIC_VGBSTAT_NETERR) {
			unsigned nerr = status >> SLIC_VGBSTAT_NERRSHFT &
				   SLIC_VGBSTAT_NERRMSK;

			if (nerr == SLIC_VGBSTAT_NCSERR)
				SLIC_INC_STATS_COUNTER(stats, rx_ipcsum);
			if (nerr == SLIC_VGBSTAT_NUFLOW)
				SLIC_INC_STATS_COUNTER(stats, rx_iplen);
			if (nerr == SLIC_VGBSTAT_NHLEN)
				SLIC_INC_STATS_COUNTER(stats, rx_iphlen);
		}
		if (status & SLIC_VGBSTAT_LNKERR) {
			unsigned lerr = status & SLIC_VGBSTAT_LERRMSK;

			if (lerr == SLIC_VGBSTAT_LDEARLY)
				SLIC_INC_STATS_COUNTER(stats, rx_early);
			if (lerr == SLIC_VGBSTAT_LBOFLO)
				SLIC_INC_STATS_COUNTER(stats, rx_buffoflow);
			if (lerr == SLIC_VGBSTAT_LCODERR)
				SLIC_INC_STATS_COUNTER(stats, rx_lcode);
			if (lerr == SLIC_VGBSTAT_LDBLNBL)
				SLIC_INC_STATS_COUNTER(stats, rx_drbl);
			if (lerr == SLIC_VGBSTAT_LCRCERR)
				SLIC_INC_STATS_COUNTER(stats, rx_crc);
			if (lerr == SLIC_VGBSTAT_LOFLO)
				SLIC_INC_STATS_COUNTER(stats, rx_oflow802);
			if (lerr == SLIC_VGBSTAT_LUFLO)
				SLIC_INC_STATS_COUNTER(stats, rx_uflow802);
		}
	}
	SLIC_INC_STATS_COUNTER(stats, rx_errors);
}

void slic_handle_receive(struct slic_device *sdev, unsigned int todo,
				unsigned int *done)
{
	struct slic_rx_queue *rxq = &sdev->rxq;
	struct net_device *dev = sdev->netdev;
	struct slic_rx_buffer *buff;
	struct slic_rx_desc *desc;
	unsigned int frames = 0;
	unsigned int bytes = 0;
	struct sk_buff *skb;
	unsigned status;
	unsigned len;

	while (todo && (rxq->done_idx != rxq->put_idx)) {
		buff = &rxq->rxbuffs[rxq->done_idx];

		skb = buff->skb;
		if (!skb)
			break;

		desc = (struct slic_rx_desc *)skb->data;

		dma_sync_single_for_cpu(&sdev->pdev->dev,
					dma_unmap_addr(buff, map_addr),
					buff->addr_offset + sizeof(*desc),
					DMA_FROM_DEVICE);

		status = le32_to_cpu(desc->status);
		if (!(status & SLIC_IRHDDR_SVALID)) {
			dma_sync_single_for_device(&sdev->pdev->dev,
						   dma_unmap_addr(buff,
								  map_addr),
						   buff->addr_offset +
						   sizeof(*desc),
						   DMA_FROM_DEVICE);
			break;
		}

		buff->skb = NULL;

		dma_unmap_single(&sdev->pdev->dev,
				 dma_unmap_addr(buff, map_addr),
				 dma_unmap_len(buff, map_len),
				 DMA_FROM_DEVICE);

		skb_reserve(skb, SLIC_RX_BUFF_HDR_SIZE);

		if (unlikely(status & SLIC_IRHDDR_ERR)) {
			slic_handle_frame_error(sdev, skb);
			dev_kfree_skb_any(skb);
		} else {
			struct ethhdr *eh = (struct ethhdr *)skb->data;

			if (is_multicast_ether_addr(eh->h_dest))
				SLIC_INC_STATS_COUNTER(&sdev->stats, rx_mcasts);

			len = le32_to_cpu(desc->length) & SLIC_IRHDDR_FLEN_MSK;
			skb_put(skb, len);
			skb->protocol = eth_type_trans(skb, dev);
			skb->ip_summed = CHECKSUM_UNNECESSARY;

			napi_gro_receive(&sdev->napi, skb);

			bytes += len;
			frames++;
		}
		rxq->done_idx = slic_next_queue_idx(rxq->done_idx, rxq->len);
		todo--;
	}

	int_stats_update_begin(&sdev->stats.syncp);
	sdev->stats.rx_bytes += bytes;
	sdev->stats.rx_packets += frames;
	int_stats_update_end(&sdev->stats.syncp);

	slic_refill_rx_queue(sdev, GFP_ATOMIC);
}

void slic_handle_link_irq(struct slic_device *sdev)
{
	struct slic_shmem *sm = &sdev->shmem;
	struct slic_shmem_data *sm_data = sm->shmem_data;
	unsigned int duplex;
	int speed;
	unsigned link;

	link = le32_to_cpu(sm_data->link);

	if (link & SLIC_GIG_LINKUP) {
		if (link & SLIC_GIG_SPEED_1000)
			speed = SPEED_1000;
		else if (link & SLIC_GIG_SPEED_100)
			speed = SPEED_100;
		else
			speed = SPEED_10;

		duplex = (link & SLIC_GIG_FULLDUPLEX) ? DUPLEX_FULL :
							DUPLEX_HALF;
	} else {
		duplex = DUPLEX_UNKNOWN;
		speed = SPEED_UNKNOWN;
	}
	slic_configure_link(sdev, speed, duplex);
}

void slic_handle_upr_irq(struct slic_device *sdev, unsigned irqs)
{
	struct slic_upr *upr;

	upr = slic_dequeue_upr(sdev);
	if (!upr) {
		netdev_warn(sdev->netdev, "no upr found on list\n");
		return;
	}

	if (upr->type == SLIC_UPR_LSTAT) {
		if (unlikely(irqs & SLIC_ISR_UPCERR_MASK)) {
			slic_queue_upr(sdev, upr);
			return;
		}
		slic_handle_link_irq(sdev);
	}
	kfree(upr);
}

int slic_handle_link_change(struct slic_device *sdev)
{
	return slic_new_upr(sdev, SLIC_UPR_LSTAT, sdev->shmem.link_paddr);
}

void slic_handle_err_irq(struct slic_device *sdev, unsigned isr)
{
	struct slic_stats *stats = &sdev->stats;

	if (isr & SLIC_ISR_RMISS)
		SLIC_INC_STATS_COUNTER(stats, rx_buff_miss);
	if (isr & SLIC_ISR_XDROP)
		SLIC_INC_STATS_COUNTER(stats, tx_dropped);
	if (!(isr & (SLIC_ISR_RMISS | SLIC_ISR_XDROP)))
		SLIC_INC_STATS_COUNTER(stats, irq_errs);
}

void slic_handle_irq(struct slic_device *sdev, unsigned isr,
			    unsigned int todo, unsigned int *done)
{
	if (isr & SLIC_ISR_ERR)
		slic_handle_err_irq(sdev, isr);

	if (isr & SLIC_ISR_LEVENT)
		slic_handle_link_change(sdev);

	if (isr & SLIC_ISR_UPC_MASK)
		slic_handle_upr_irq(sdev, isr);

	if (isr & SLIC_ISR_RCV)
		slic_handle_receive(sdev, todo, done);

	if (isr & SLIC_ISR_CMD)
		slic_xmit_complete(sdev);
}

int slic_poll(struct napi_struct *napi, int todo)
{
	struct slic_device *sdev = container_of(napi, struct slic_device, napi);
	struct slic_shmem *sm = &sdev->shmem;
	struct slic_shmem_data *sm_data = sm->shmem_data;
	unsigned isr = le32_to_cpu(sm_data->isr);
	int done = 0;

	slic_handle_irq(sdev, isr, todo, &done);

	if (done < todo) {
		napi_complete_done(napi, done);
		sm_data->isr = 0;
		wmb();
		slic_write(sdev, SLIC_REG_ISR, 0);
		slic_flush_write(sdev);
	}

	return done;
}


void slic_card_reset(struct slic_device *sdev)
{
	unsigned short cmd;

	slic_write(sdev, SLIC_REG_RESET, SLIC_RESET_MAGIC);
	pci_read_config_word(sdev->pdev, PCI_COMMAND, &cmd);
	mdelay(1);
}

int slic_init_stat_queue(struct slic_device *sdev)
{
	unsigned int DESC_ALIGN_MASK = SLIC_STATS_DESC_ALIGN - 1;
	struct slic_stat_queue *stq = &sdev->stq;
	struct slic_stat_desc *descs;
	unsigned int misalign;
	unsigned int offset;
	struct dma_addr_t paddr;
	size_t size;
	int err;
	int i;

	stq->len = SLIC_NUM_STAT_DESCS;
	stq->active_array = 0;
	stq->done_idx = 0;

	size = stq->len * sizeof(*descs) + DESC_ALIGN_MASK;

	for (i = 0; i < SLIC_NUM_STAT_DESC_ARRAYS; i++) {
		descs = dma_alloc_coherent(&sdev->pdev->dev, size, &paddr,
					   GFP_KERNEL);
		if (!descs) {
			netdev_err(sdev->netdev,
				   "failed to allocate status descriptors\n");
			err = -ENOMEM;
		}
		offset = 0;
		misalign = paddr & DESC_ALIGN_MASK;
		if (misalign) {
			offset = SLIC_STATS_DESC_ALIGN - misalign;
			descs += offset;
			paddr += offset;
		}

		slic_write(sdev, SLIC_REG_RBAR, lower_32_bits(paddr) |
						stq->len);
		stq->descs[i] = descs;
		stq->paddr[i] = paddr;
		stq->addr_offset[i] = offset;
	}

	stq->mem_size = size;

	while (i--) {
		dma_free_coherent(&sdev->pdev->dev, stq->mem_size,
				  stq->descs[i] - stq->addr_offset[i],
				  stq->paddr[i] - stq->addr_offset[i]);
	}

	return err;
}

void slic_free_stat_queue(struct slic_device *sdev)
{
	struct slic_stat_queue *stq = &sdev->stq;
	int i;

	for (i = 0; i < SLIC_NUM_STAT_DESC_ARRAYS; i++) {
		dma_free_coherent(&sdev->pdev->dev, stq->mem_size,
				  stq->descs[i] - stq->addr_offset[i],
				  stq->paddr[i] - stq->addr_offset[i]);
	}
}

int slic_init_tx_queue(struct slic_device *sdev)
{
	struct slic_tx_queue *txq = &sdev->txq;
	struct slic_tx_buffer *buff;
	struct slic_tx_desc *desc;
	unsigned int i;
	int err;

	txq->len = SLIC_NUM_TX_DESCS;
	txq->put_idx = 0;
	txq->done_idx = 0;

	txq->txbuffs = kcalloc(txq->len, sizeof(*buff), GFP_KERNEL);
	if (!txq->txbuffs)
		return -ENOMEM;

	txq->dma_pool = dma_pool_create("slic_pool", &sdev->pdev->dev,
					sizeof(*desc), SLIC_TX_DESC_ALIGN,
					4096);
	if (!txq->dma_pool) {
		err = -ENOMEM;
		netdev_err(sdev->netdev, "failed to create dma pool\n");
	}

	for (i = 0; i < txq->len; i++) {
		buff = &txq->txbuffs[i];
		desc = dma_pool_zalloc(txq->dma_pool, GFP_KERNEL,
				       &buff->desc_paddr);
		if (!desc) {
			netdev_err(sdev->netdev,
				   "failed to alloc pool chunk (%i)\n", i);
			err = -ENOMEM;
		}

		desc->hnd = cpu_to_le32((unsigned)(i + 1));
		desc->cmd = SLIC_CMD_XMT_REQ;
		desc->flags = 0;
		desc->type = cpu_to_le32(SLIC_CMD_TYPE_DUMB);
		buff->desc = desc;
	}

	while (i--) {
		buff = &txq->txbuffs[i];
		dma_pool_free(txq->dma_pool, buff->desc, buff->desc_paddr);
	}
	dma_pool_destroy(txq->dma_pool);

	kfree(txq->txbuffs);

	return err;
}

void slic_free_tx_queue(struct slic_device *sdev)
{
	struct slic_tx_queue *txq = &sdev->txq;
	struct slic_tx_buffer *buff;
	unsigned int i;

	for (i = 0; i < txq->len; i++) {
		buff = &txq->txbuffs[i];
		dma_pool_free(txq->dma_pool, buff->desc, buff->desc_paddr);
		if (!buff->skb)
			continue;

		dma_unmap_single(&sdev->pdev->dev,
				 dma_unmap_addr(buff, map_addr),
				 dma_unmap_len(buff, map_len), DMA_TO_DEVICE);
		consume_skb(buff->skb);
	}
	dma_pool_destroy(txq->dma_pool);

	kfree(txq->txbuffs);
}

int slic_init_rx_queue(struct slic_device *sdev)
{
	struct slic_rx_queue *rxq = &sdev->rxq;
	struct slic_rx_buffer *buff;

	rxq->len = SLIC_NUM_RX_LES;
	rxq->done_idx = 0;
	rxq->put_idx = 0;

	buff = kcalloc(rxq->len, sizeof(*buff), GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	rxq->rxbuffs = buff;
	slic_refill_rx_queue(sdev, GFP_KERNEL);

	return 0;
}

void slic_free_rx_queue(struct slic_device *sdev)
{
	struct slic_rx_queue *rxq = &sdev->rxq;
	struct slic_rx_buffer *buff;
	unsigned int i;

	for (i = 0; i < rxq->len; i++) {
		buff = &rxq->rxbuffs[i];

		if (!buff->skb)
			continue;

		dma_unmap_single(&sdev->pdev->dev,
				 dma_unmap_addr(buff, map_addr),
				 dma_unmap_len(buff, map_len),
				 DMA_FROM_DEVICE);
		consume_skb(buff->skb);
	}
	kfree(rxq->rxbuffs);
}

void slic_set_link_autoneg(struct slic_device *sdev)
{
	unsigned int subid = sdev->pdev->subsystem_device;
	unsigned val;

	if (sdev->is_fiber) {
		val = MII_ADVERTISE << 16 | ADVERTISE_1000XFULL |
		      ADVERTISE_1000XPAUSE | ADVERTISE_1000XPSE_ASYM;
		slic_write(sdev, SLIC_REG_WPHY, val);
		val = MII_BMCR << 16 | BMCR_RESET | BMCR_ANENABLE |
		      BMCR_ANRESTART;
		slic_write(sdev, SLIC_REG_WPHY, val);
	} else {		val = MII_ADVERTISE << 16 | ADVERTISE_100FULL |
		      ADVERTISE_100HALF | ADVERTISE_10FULL | ADVERTISE_10HALF;
		val |= ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM;
		val |= ADVERTISE_CSMA;
		slic_write(sdev, SLIC_REG_WPHY, val);

		al = MII_CTRL1000 << 16 | ADVERTISE_1000FULL;
		slic_write(sdev, SLIC_REG_WPHY, val);

		if (subid != PCI_SUBDEVICE_ID_ALACRITECH_CICADA) {
			val = SLIC_MIICR_REG_16 | SLIC_MRV_REG16_XOVERON;
			slic_write(sdev, SLIC_REG_WPHY, val);

			val = MII_BMCR << 16 | BMCR_RESET | BMCR_ANENABLE |
			      BMCR_ANRESTART;
			slic_write(sdev, SLIC_REG_WPHY, val);
		} else {
			val = MII_BMCR << 16 | BMCR_ANENABLE | BMCR_ANRESTART;
			slic_write(sdev, SLIC_REG_WPHY, val);
		}
	}
}

void slic_set_mac_address(struct slic_device *sdev)
{
	unsigned char *addr = sdev->netdev->dev_addr;
	unsigned val;

	val = addr[5] | addr[4] << 8 | addr[3] << 16 | addr[2] << 24;

	slic_write(sdev, SLIC_REG_WRADDRAL, val);
	slic_write(sdev, SLIC_REG_WRADDRBL, val);

	val = addr[0] << 8 | addr[1];

	slic_write(sdev, SLIC_REG_WRADDRAH, val);
	slic_write(sdev, SLIC_REG_WRADDRBH, val);
	slic_flush_write(sdev);
}

MODULE_FIRMWARE(SLIC_RCV_FIRMWARE_MOJAVE);
MODULE_FIRMWARE(SLIC_RCV_FIRMWARE_OASIS);

MODULE_FIRMWARE(SLIC_FIRMWARE_MOJAVE);
MODULE_FIRMWARE(SLIC_FIRMWARE_OASIS);

int slic_load_firmware(struct slic_device *sdev)
{
	unsigned* sectstart;
	unsigned* sectsize;
	struct firmware *fw;
	unsigned int datalen;
	char *file;
	int code_start;
	unsigned int i;
	unsigned numsects;
	int idx = 0;
	unsigned sect;
	unsigned instr;
	unsigned addr;
	unsigned base;
	int err;

	file = (sdev->model == SLIC_MODEL_OASIS) ?  SLIC_FIRMWARE_OASIS :
						    SLIC_FIRMWARE_MOJAVE;
	err = request_firmware(&fw, file, &sdev->pdev->dev);
	if (err) {
		dev_err(&sdev->pdev->dev, "failed to load firmware %s\n", file);
		return err;
	}
	if (fw->size < SLIC_FIRMWARE_MIN_SIZE) {
		dev_err(&sdev->pdev->dev,
			"invalid firmware size %zu (min is %u)\n", fw->size,
			SLIC_FIRMWARE_MIN_SIZE);
		err = -EINVAL;
	}

	numsects = slic_read_dword_from_firmware(fw, &idx);
	if (numsects == 0 || numsects > SLIC_FIRMWARE_MAX_SECTIONS) {
		dev_err(&sdev->pdev->dev,
			"invalid number of sections in firmware: %u", numsects);
		err = -EINVAL;
	}

	datalen = numsects * 8 + 4;
	for (i = 0; i < numsects; i++) {
		sectsize[i] = slic_read_dword_from_firmware(fw, &idx);
		datalen += sectsize[i];
	}

	if (datalen > fw->size) {
		dev_err(&sdev->pdev->dev,
			"invalid firmware size %zu (expected >= %u)\n",
			fw->size, datalen);
		err = -EINVAL;
	}
	for (i = 0; i < numsects; i++)
		sectstart[i] = slic_read_dword_from_firmware(fw, &idx);

	code_start = idx;
	instr = slic_read_dword_from_firmware(fw, &idx);

	for (sect = 0; sect < numsects; sect++) {
		unsigned int ssize = sectsize[sect] >> 3;

		base = sectstart[sect];

		for (addr = 0; addr < ssize; addr++) {
			slic_write(sdev, SLIC_REG_WCS, base + addr);
			slic_write(sdev, SLIC_REG_WCS, instr);
			instr = slic_read_dword_from_firmware(fw, &idx);
			slic_write(sdev, SLIC_REG_WCS, instr);
			instr = slic_read_dword_from_firmware(fw, &idx);
		}
	}

	idx = code_start;

	for (sect = 0; sect < numsects; sect++) {
		unsigned int ssize = sectsize[sect] >> 3;

		instr = slic_read_dword_from_firmware(fw, &idx);
		base = sectstart[sect];
		if (base < 0x8000)
			continue;

		for (addr = 0; addr < ssize; addr++) {
			slic_write(sdev, SLIC_REG_WCS,
				   SLIC_WCS_COMPARE | (base + addr));
			slic_write(sdev, SLIC_REG_WCS, instr);
			instr = slic_read_dword_from_firmware(fw, &idx);
			slic_write(sdev, SLIC_REG_WCS, instr);
			instr = slic_read_dword_from_firmware(fw, &idx);
		}
	}
	slic_flush_write(sdev);
	mdelay(10);
	slic_write(sdev, SLIC_REG_WCS, SLIC_WCS_START);
	slic_flush_write(sdev);
	mdelay(20);
	release_firmware(fw);

	return err;
}

void slic_free_shmem(struct slic_device *sdev)
{
	struct slic_shmem *sm = &sdev->shmem;
	struct slic_shmem_data *sm_data = sm->shmem_data;

	dma_free_coherent(&sdev->pdev->dev, sizeof(*sm_data), sm_data,
			  sm->isr_paddr);
}


int slic_open(struct net_device *dev)
{
	struct slic_device *sdev = netdev_priv(dev);
	int err;

	netif_carrier_off(dev);

	err = slic_init_iface(sdev);
	if (err) {
		netdev_err(dev, "failed to initialize interface: %i\n", err);
		return err;
	}

	netif_start_queue(dev);

	return 0;
}

int slic_close(struct net_device *dev)
{
	struct slic_device *sdev = netdev_priv(dev);
	unsigned val;

	netif_stop_queue(dev);

	napi_disable(&sdev->napi);
	slic_write(sdev, SLIC_REG_ICR, SLIC_ICR_INT_OFF);
	slic_write(sdev, SLIC_REG_ISR, 0);
	slic_flush_write(sdev);

	free_irq(sdev->pdev->irq, sdev);
	val = SLIC_GXCR_RESET | SLIC_GXCR_PAUSEEN;
	slic_write(sdev, SLIC_REG_WXCFG, val);

	val = SLIC_GRCR_RESET | SLIC_GRCR_CTLEN | SLIC_GRCR_ADDRAEN |
	      SLIC_GRCR_HASHSIZE << SLIC_GRCR_HASHSIZE_SHIFT;
	slic_write(sdev, SLIC_REG_WRCFG, val);

	val = MII_BMCR << 16 | BMCR_PDOWN;
	slic_write(sdev, SLIC_REG_WPHY, val);
	slic_flush_write(sdev);

	slic_clear_upr_list(&sdev->upr_list);
	slic_write(sdev, SLIC_REG_QUIESCE, 0);

	slic_free_stat_queue(sdev);
	slic_free_tx_queue(sdev);
	slic_free_rx_queue(sdev);
	slic_free_shmem(sdev);

	slic_card_reset(sdev);
	netif_carrier_off(dev);

	return 0;
}

void slic_get_stats(struct net_device *dev,
			   struct rtnl_link_stats64 *lst)
{
	struct slic_device *sdev = netdev_priv(dev);
	struct slic_stats *stats = &sdev->stats;

	SLIC_GET_STATS_COUNTER(lst->rx_packets, stats, rx_packets);
	SLIC_GET_STATS_COUNTER(lst->tx_packets, stats, tx_packets);
	SLIC_GET_STATS_COUNTER(lst->rx_bytes, stats, rx_bytes);
	SLIC_GET_STATS_COUNTER(lst->tx_bytes, stats, tx_bytes);
	SLIC_GET_STATS_COUNTER(lst->rx_errors, stats, rx_errors);
	SLIC_GET_STATS_COUNTER(lst->rx_dropped, stats, rx_buff_miss);
	SLIC_GET_STATS_COUNTER(lst->tx_dropped, stats, tx_dropped);
	SLIC_GET_STATS_COUNTER(lst->multicast, stats, rx_mcasts);
	SLIC_GET_STATS_COUNTER(lst->rx_over_errors, stats, rx_buffoflow);
	SLIC_GET_STATS_COUNTER(lst->rx_crc_errors, stats, rx_crc);
	SLIC_GET_STATS_COUNTER(lst->rx_fifo_errors, stats, rx_oflow802);
	SLIC_GET_STATS_COUNTER(lst->tx_carrier_errors, stats, tx_carrier);
}
unsigned short slic_eeprom_csum(unsigned char *eeprom, unsigned int len)
{
	unsigned char *ptr = eeprom;
	unsigned csum = 0;
	unsigned short data;

	while (len > 1) {
		memcpy(&data, ptr, sizeof(data));
		csum += le16_to_cpu(data);
		ptr += 2;
		len -= 2;
	}
	if (len > 0)
		csum += *(unsigned char *)ptr;
	while (csum >> 16)
		csum = (csum & 0xFFFF) + ((csum >> 16) & 0xFFFF);
	return ~csum;
}

int slic_read_eeprom(struct slic_device *sdev)
{
	unsigned int devfn = PCI_FUNC(sdev->pdev->devfn);
	struct slic_shmem *sm = &sdev->shmem;
	struct slic_shmem_data *sm_data = sm->shmem_data;
	unsigned int MAX_LOOPS = 5000;
	unsigned int codesize;
	unsigned char *eeprom;
	struct slic_upr *upr;
	unsigned int i = 0;
	int err = 0;
	unsigned char **mac;

	eeprom = dma_alloc_coherent(&sdev->pdev->dev, SLIC_EEPROM_SIZE,
				    &paddr, GFP_KERNEL);
	if (!eeprom)
		return -ENOMEM;

	slic_write(sdev, SLIC_REG_ICR, SLIC_ICR_INT_OFF);
	slic_write(sdev, SLIC_REG_ISP, lower_32_bits(sm->isr_paddr));

	err = slic_new_upr(sdev, SLIC_UPR_CONFIG, paddr);
	if (!err) {
		for (i = 0; i < MAX_LOOPS; i++) {
			if (le32_to_cpu(sm_data->isr) & SLIC_ISR_UPC)
				break;
			mdelay(1);
		}
		if (i == MAX_LOOPS) {
			dev_err(&sdev->pdev->dev,
				"timed out while waiting for eeprom data\n");
			err = -ETIMEDOUT;
		}
		upr = slic_dequeue_upr(sdev);
		kfree(upr);
	}

	slic_write(sdev, SLIC_REG_ISP, 0);
	slic_write(sdev, SLIC_REG_ISR, 0);
	slic_flush_write(sdev);

	if (sdev->model == SLIC_MODEL_OASIS) {
		struct slic_oasis_eeprom *oee;

		oee = (struct slic_oasis_eeprom *)eeprom;
		mac[0] = oee->mac;
		mac[1] = oee->mac2;
		codesize = le16_to_cpu(oee->eeprom_code_size);
	} else {
		struct slic_mojave_eeprom *mee;

		mee = (struct slic_mojave_eeprom *)eeprom;
		mac[0] = mee->mac;
		mac[1] = mee->mac2;
		codesize = le16_to_cpu(mee->eeprom_code_size);
	}

	if (!slic_eeprom_valid(eeprom, codesize)) {
		dev_err(&sdev->pdev->dev, "invalid checksum in eeprom\n");
		err = -EINVAL;
	}
	eth_hw_addr_set(sdev->netdev, mac[devfn]);
	dma_free_coherent(&sdev->pdev->dev, SLIC_EEPROM_SIZE, eeprom, paddr);

	return err;
}

module_pci_driver(slic_driver);

MODULE_DESCRIPTION("Alacritech non-accelerated SLIC driver");
MODULE_AUTHOR("Lino Sanfilippo <LinoSanfilippo@gmx.de>");
MODULE_LICENSE("GPL");
