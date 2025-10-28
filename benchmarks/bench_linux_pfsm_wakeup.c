char ** dev_pfsm = {PMIC_A, PMIC_B, PMIC_C};

int main(int argc, char **argv)
{
	int i, ret, fd_rtc, fd_pfsm = { 0 };
	struct rtc_time rtc_tm;
	struct pmic_state_opt pmic_opt = { 0 };
	unsigned long data;

	fd_rtc = open(RTC_A, O_RDONLY);
	if (fd_rtc < 0) {
		perror("Failed to open RTC device.");
	}

	for (i = 0 ; i < PMIC_NB ; i++) {
		fd_pfsm[i] = open(dev_pfsm[i], O_RDWR);
		if (fd_pfsm[i] < 0) {
			perror("Failed to open PFSM device.");
		}
	}

	ret = ioctl(fd_rtc, RTC_RD_TIME, &rtc_tm);
	if (ret < 0) {
		perror("Failed to read RTC date/time.");
	}
	printf("Current RTC date/time is %d-%d-%d, %02d:%02d:%02d.\n",
	       rtc_tm.tm_mday, rtc_tm.tm_mon + 1, rtc_tm.tm_year + 1900,
	       rtc_tm.tm_hour, rtc_tm.tm_min, rtc_tm.tm_sec);

	rtc_tm.tm_sec += ALARM_DELTA_SEC;
	if (rtc_tm.tm_sec >= 60) {
		rtc_tm.tm_sec %= 60;
		rtc_tm.tm_min++;
	}
	if (rtc_tm.tm_min == 60) {
		rtc_tm.tm_min = 0;
		rtc_tm.tm_hour++;
	}
	if (rtc_tm.tm_hour == 24)
		rtc_tm.tm_hour = 0;
	ret = ioctl(fd_rtc, RTC_ALM_SET, &rtc_tm);
	if (ret < 0) {
		perror("Failed to set RTC alarm.");
	}

	ret = ioctl(fd_rtc, RTC_AIE_ON, 0);
	if (ret < 0) {
		perror("Failed to enable alarm interrupts.");
	}
	printf("Waiting %d seconds for alarm...\n", ALARM_DELTA_SEC);

	pmic_opt.ddr_retention = 1;
	for (i = PMIC_NB - 1 ; i >= 0 ; i--) {
		printf("Set RETENTION state for PMIC_%d.\n", i);
		sleep(1);
		ret = ioctl(fd_pfsm[i], PMIC_SET_RETENTION_STATE, &pmic_opt);
		if (ret < 0) {
			perror("Failed to set RETENTION state.");
		}
	}

	ret = read(fd_rtc, &data, sizeof(unsigned long));
	if (ret < 0){
		perror("Failed to get RTC alarm.");
	}else
		puts("Alarm rang.\n");

	ioctl(fd_rtc, RTC_AIE_OFF, 0);

	ioctl(fd_pfsm[0], PMIC_SET_ACTIVE_STATE, 0);

	for (i = 0 ; i < PMIC_NB ; i++)
		if (fd_pfsm[i])
			close(fd_pfsm[i]);

	if (fd_rtc)
		close(fd_rtc);

	return 0;
}
