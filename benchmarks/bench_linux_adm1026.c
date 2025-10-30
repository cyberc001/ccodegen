#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon-vid.h>
#include <linux/err.h>
#include <linux/mutex.h>

unsigned short *normal_i2c = { 0x2c, 0x2d, 0x2e, I2C_CLIENT_END };

int* gpio_input = { -1, -1, -1, -1, -1, -1, -1, -1, -1,
				-1, -1, -1, -1, -1, -1, -1, -1 };
int* gpio_output = { -1, -1, -1, -1, -1, -1, -1, -1, -1,
				-1, -1, -1, -1, -1, -1, -1, -1 };
int* gpio_inverted = { -1, -1, -1, -1, -1, -1, -1, -1, -1,
				-1, -1, -1, -1, -1, -1, -1, -1 };
int* gpio_normal = { -1, -1, -1, -1, -1, -1, -1, -1, -1,
				-1, -1, -1, -1, -1, -1, -1, -1 };
int* gpio_fan = { -1, -1, -1, -1, -1, -1, -1, -1 };
module_param_array(gpio_input, int, NULL, 0);
MODULE_PARM_DESC(gpio_input, "List of GPIO pins (0-16) to program as inputs");
module_param_array(gpio_output, int, NULL, 0);
MODULE_PARM_DESC(gpio_output,
		 "List of GPIO pins (0-16) to program as outputs");
module_param_array(gpio_inverted, int, NULL, 0);
MODULE_PARM_DESC(gpio_inverted,
		 "List of GPIO pins (0-16) to program as inverted");
module_param_array(gpio_normal, int, NULL, 0);
MODULE_PARM_DESC(gpio_normal,
		 "List of GPIO pins (0-16) to program as normal/non-inverted");
module_param_array(gpio_fan, int, NULL, 0);
MODULE_PARM_DESC(gpio_fan, "List of GPIO pins (0-7) to program as fan tachs");


#define ADM1026_REG_CONFIG1	0x00
#define CFG1_MONITOR		0x01
#define CFG1_INT_ENABLE		0x02
#define CFG1_INT_CLEAR		0x04
#define CFG1_AIN8_9		0x08
#define CFG1_THERM_HOT		0x10
#define CFG1_DAC_AFC		0x20
#define CFG1_PWM_AFC		0x40
#define CFG1_RESET		0x80

#define ADM1026_REG_CONFIG2	0x01

#define ADM1026_REG_CONFIG3	0x07
#define CFG3_GPIO16_ENABLE	0x01
#define CFG3_CI_CLEAR		0x02
#define CFG3_VREF_250		0x04
#define CFG3_GPIO16_DIR		0x40
#define CFG3_GPIO16_POL		0x80

#define ADM1026_REG_E2CONFIG	0x13
#define E2CFG_READ		0x01
#define E2CFG_WRITE		0x02
#define E2CFG_ERASE		0x04
#define E2CFG_ROM		0x08
#define E2CFG_CLK_EXT		0x80

u16* ADM1026_REG_IN = {
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
		0x36, 0x37, 0x27, 0x29, 0x26, 0x2a,
		0x2b, 0x2c, 0x2d, 0x2e, 0x2f
	};
u16* ADM1026_REG_IN_MIN = {
		0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d,
		0x5e, 0x5f, 0x6d, 0x49, 0x6b, 0x4a,
		0x4b, 0x4c, 0x4d, 0x4e, 0x4f
	};
u16* ADM1026_REG_IN_MAX = {
		0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
		0x56, 0x57, 0x6c, 0x41, 0x6a, 0x42,
		0x43, 0x44, 0x45, 0x46, 0x47
	};

u16* ADM1026_REG_TEMP = { 0x1f, 0x28, 0x29 };
u16* ADM1026_REG_TEMP_MIN = { 0x69, 0x48, 0x49 };
u16* ADM1026_REG_TEMP_MAX = { 0x68, 0x40, 0x41 };
u16* ADM1026_REG_TEMP_TMIN = { 0x10, 0x11, 0x12 };
u16* ADM1026_REG_TEMP_THERM = { 0x0d, 0x0e, 0x0f };
u16* ADM1026_REG_TEMP_OFFSET = { 0x1e, 0x6e, 0x6f };

int* adm1026_scaling = {
		2250, 2250, 2250, 2250, 2250, 2250,
		1875, 1875, 1875, 1875, 3000, 3330,
		3330, 4995, 2250, 12000, 13875
	};

struct pwm_data {
	unsigned char pwm;
	unsigned char enable;
	unsigned char auto_pwm_min;
};

struct adm1026_data {
	struct i2c_client *client;
	struct attribute_group **groups;

	struct mutex update_lock;
	int valid;
	unsigned long last_reading;
	unsigned long last_config;

	unsigned char* in;		
	unsigned char* in_max;		
	unsigned char* in_min;		
	s8* temp;		
	s8* temp_min;		
	s8* temp_max;		
	s8* temp_tmin;	
	s8* temp_crit;
	s8* temp_offset;
	unsigned char* fan;	
	unsigned char* fan_min;
	unsigned char* fan_div;
	struct pwm_data pwm1;
	unsigned char vrm;		
	unsigned char analog_out;
	long alarms;	
	long alarm_mask;
	long gpio;
	long gpio_mask;
	unsigned char* gpio_config;
	unsigned char config1;	
	unsigned char config2;
	unsigned char config3;
};

int adm1026_read_value(struct i2c_client *client, unsigned char reg)
{
	int res;

	if (reg < 0x80) {
		res = i2c_smbus_read_byte_data(client, reg) & 0xff;
	} else {
		res = 0;
	}
	return res;
}

int adm1026_write_value(struct i2c_client *client, unsigned char reg, int value)
{
	int res;

	if (reg < 0x80) {
		res = i2c_smbus_write_byte_data(client, reg, value);
	} else {
		res = 0;
	}
	return res;
}

struct adm1026_data *adm1026_update_device(struct device *dev)
{
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int i;
	long value, alarms, gpio;

	mutex_lock(&data->update_lock);
	if (!data->valid
	    || time_after(jiffies,
			  data->last_reading + ADM1026_DATA_INTERVAL)) {
		dev_dbg(&client->dev, "Reading sensor values\n");
		for (i = 0; i <= 16; ++i) {
			data->in[i] =
			    adm1026_read_value(client, ADM1026_REG_IN[i]);
		}

		for (i = 0; i <= 7; ++i) {
			data->fan[i] =
			    adm1026_read_value(client, ADM1026_REG_FAN(i));
		}

		for (i = 0; i <= 2; ++i) {
			data->temp[i] =
			    adm1026_read_value(client, ADM1026_REG_TEMP[i]);
		}

		data->pwm1.pwm = adm1026_read_value(client,
			ADM1026_REG_PWM);
		data->analog_out = adm1026_read_value(client,
			ADM1026_REG_DAC);
		alarms = adm1026_read_value(client, ADM1026_REG_STATUS4);
		gpio = alarms & 0x80 ? 0x0100 : 0;
		alarms &= 0x7f;
		alarms <<= 8;
		alarms |= adm1026_read_value(client, ADM1026_REG_STATUS3);
		alarms <<= 8;
		alarms |= adm1026_read_value(client, ADM1026_REG_STATUS2);
		alarms <<= 8;
		alarms |= adm1026_read_value(client, ADM1026_REG_STATUS1);
		data->alarms = alarms;

		gpio |= adm1026_read_value(client,
			ADM1026_REG_GPIO_STATUS_8_15);
		gpio <<= 8;
		gpio |= adm1026_read_value(client,
			ADM1026_REG_GPIO_STATUS_0_7);
		data->gpio = gpio;

		data->last_reading = jiffies;

	if (!data->valid ||
	    time_after(jiffies, data->last_config + ADM1026_CONFIG_INTERVAL)) {
		dev_dbg(&client->dev, "Reading config values\n");
		for (i = 0; i <= 16; ++i) {
			data->in_min[i] = adm1026_read_value(client,
				ADM1026_REG_IN_MIN[i]);
			data->in_max[i] = adm1026_read_value(client,
				ADM1026_REG_IN_MAX[i]);
		}

		value = adm1026_read_value(client, ADM1026_REG_FAN_DIV_0_3)
			| (adm1026_read_value(client, ADM1026_REG_FAN_DIV_4_7)
			<< 8);
		for (i = 0; i <= 7; ++i) {
			data->fan_min[i] = adm1026_read_value(client,
				ADM1026_REG_FAN_MIN(i));
			data->fan_div[i] = DIV_FROM_REG(value & 0x03);
			value >>= 2;
		}

		for (i = 0; i <= 2; ++i) {
			data->temp_min[i] = adm1026_read_value(client,
				ADM1026_REG_TEMP_MIN[i]);
			data->temp_max[i] = adm1026_read_value(client,
				ADM1026_REG_TEMP_MAX[i]);
			data->temp_tmin[i] = adm1026_read_value(client,
				ADM1026_REG_TEMP_TMIN[i]);
			data->temp_crit[i] = adm1026_read_value(client,
				ADM1026_REG_TEMP_THERM[i]);
			data->temp_offset[i] = adm1026_read_value(client,
				ADM1026_REG_TEMP_OFFSET[i]);
		}

		alarms = adm1026_read_value(client, ADM1026_REG_MASK4);
		gpio = alarms & 0x80 ? 0x0100 : 0;
		alarms = (alarms & 0x7f) << 8;
		alarms |= adm1026_read_value(client, ADM1026_REG_MASK3);
		alarms <<= 8;
		alarms |= adm1026_read_value(client, ADM1026_REG_MASK2);
		alarms <<= 8;
		alarms |= adm1026_read_value(client, ADM1026_REG_MASK1);
		data->alarm_mask = alarms;

		gpio |= adm1026_read_value(client,
			ADM1026_REG_GPIO_MASK_8_15);
		gpio <<= 8;
		gpio |= adm1026_read_value(client, ADM1026_REG_GPIO_MASK_0_7);
		data->gpio_mask = gpio;

		data->config1 = adm1026_read_value(client,
			ADM1026_REG_CONFIG1);
		if (data->config1 & CFG1_PWM_AFC) {
			data->pwm1.enable = 2;
			data->pwm1.auto_pwm_min =
				PWM_MIN_FROM_REG(data->pwm1.pwm);
		}
		data->config2 = adm1026_read_value(client,
			ADM1026_REG_CONFIG2);
		data->config3 = adm1026_read_value(client,
			ADM1026_REG_CONFIG3);
		data->gpio_config[16] = (data->config3 >> 6) & 0x03;

		value = 0;
		for (i = 0; i <= 15; ++i) {
			if ((i & 0x03) == 0) {
				value = adm1026_read_value(client,
					    ADM1026_REG_GPIO_CFG_0_3 + i/4);
			}
			data->gpio_config[i] = value & 0x03;
			value >>= 2;
		}

		data->last_config = jiffies;
	}	

	data->valid = true;
	mutex_unlock(&data->update_lock);
	return data;
}

unsigned long in_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", INS_FROM_REG(nr, data->in[nr]));
}
unsigned long in_min_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", INS_FROM_REG(nr, data->in_min[nr]));
}
unsigned long in_min_store(struct device *dev, struct device_attribute *attr,
			    char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->in_min[nr] = INS_TO_REG(nr, val);
	adm1026_write_value(client, ADM1026_REG_IN_MIN[nr], data->in_min[nr]);
	mutex_unlock(&data->update_lock);
	return count;
}
unsigned long in_max_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", INS_FROM_REG(nr, data->in_max[nr]));
}
unsigned long in_max_store(struct device *dev, struct device_attribute *attr,
			    char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->in_max[nr] = INS_TO_REG(nr, val);
	adm1026_write_value(client, ADM1026_REG_IN_MAX[nr], data->in_max[nr]);
	mutex_unlock(&data->update_lock);
	return count;
}

SENSOR_DEVICE_ATTR_RO(in0_input, in, 0);
SENSOR_DEVICE_ATTR_RW(in0_min, in_min, 0);
SENSOR_DEVICE_ATTR_RW(in0_max, in_max, 0);
SENSOR_DEVICE_ATTR_RO(in1_input, in, 1);
SENSOR_DEVICE_ATTR_RW(in1_min, in_min, 1);
SENSOR_DEVICE_ATTR_RW(in1_max, in_max, 1);
SENSOR_DEVICE_ATTR_RO(in2_input, in, 2);
SENSOR_DEVICE_ATTR_RW(in2_min, in_min, 2);
SENSOR_DEVICE_ATTR_RW(in2_max, in_max, 2);
SENSOR_DEVICE_ATTR_RO(in3_input, in, 3);
SENSOR_DEVICE_ATTR_RW(in3_min, in_min, 3);
SENSOR_DEVICE_ATTR_RW(in3_max, in_max, 3);
SENSOR_DEVICE_ATTR_RO(in4_input, in, 4);
SENSOR_DEVICE_ATTR_RW(in4_min, in_min, 4);
SENSOR_DEVICE_ATTR_RW(in4_max, in_max, 4);
SENSOR_DEVICE_ATTR_RO(in5_input, in, 5);
SENSOR_DEVICE_ATTR_RW(in5_min, in_min, 5);
SENSOR_DEVICE_ATTR_RW(in5_max, in_max, 5);
SENSOR_DEVICE_ATTR_RO(in6_input, in, 6);
SENSOR_DEVICE_ATTR_RW(in6_min, in_min, 6);
SENSOR_DEVICE_ATTR_RW(in6_max, in_max, 6);
SENSOR_DEVICE_ATTR_RO(in7_input, in, 7);
SENSOR_DEVICE_ATTR_RW(in7_min, in_min, 7);
SENSOR_DEVICE_ATTR_RW(in7_max, in_max, 7);
SENSOR_DEVICE_ATTR_RO(in8_input, in, 8);
SENSOR_DEVICE_ATTR_RW(in8_min, in_min, 8);
SENSOR_DEVICE_ATTR_RW(in8_max, in_max, 8);
SENSOR_DEVICE_ATTR_RO(in9_input, in, 9);
SENSOR_DEVICE_ATTR_RW(in9_min, in_min, 9);
SENSOR_DEVICE_ATTR_RW(in9_max, in_max, 9);
SENSOR_DEVICE_ATTR_RO(in10_input, in, 10);
SENSOR_DEVICE_ATTR_RW(in10_min, in_min, 10);
SENSOR_DEVICE_ATTR_RW(in10_max, in_max, 10);
SENSOR_DEVICE_ATTR_RO(in11_input, in, 11);
SENSOR_DEVICE_ATTR_RW(in11_min, in_min, 11);
SENSOR_DEVICE_ATTR_RW(in11_max, in_max, 11);
SENSOR_DEVICE_ATTR_RO(in12_input, in, 12);
SENSOR_DEVICE_ATTR_RW(in12_min, in_min, 12);
SENSOR_DEVICE_ATTR_RW(in12_max, in_max, 12);
SENSOR_DEVICE_ATTR_RO(in13_input, in, 13);
SENSOR_DEVICE_ATTR_RW(in13_min, in_min, 13);
SENSOR_DEVICE_ATTR_RW(in13_max, in_max, 13);
SENSOR_DEVICE_ATTR_RO(in14_input, in, 14);
SENSOR_DEVICE_ATTR_RW(in14_min, in_min, 14);
SENSOR_DEVICE_ATTR_RW(in14_max, in_max, 14);
SENSOR_DEVICE_ATTR_RO(in15_input, in, 15);
SENSOR_DEVICE_ATTR_RW(in15_min, in_min, 15);
SENSOR_DEVICE_ATTR_RW(in15_max, in_max, 15);

unsigned long in16_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", INS_FROM_REG(16, data->in[16]) -
		NEG12_OFFSET);
}
unsigned long in16_min_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", INS_FROM_REG(16, data->in_min[16])
		- NEG12_OFFSET);
}
unsigned long in16_min_store(struct device *dev,
			      struct device_attribute *attr, char *buf,
			      size_t count)
{
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->in_min[16] = INS_TO_REG(16,
				      clamp_val(val, INT_MIN,
						INT_MAX - NEG12_OFFSET) +
				      NEG12_OFFSET);
	adm1026_write_value(client, ADM1026_REG_IN_MIN[16], data->in_min[16]);
	mutex_unlock(&data->update_lock);
	return count;
}
unsigned long in16_max_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", INS_FROM_REG(16, data->in_max[16])
			- NEG12_OFFSET);
}
unsigned long in16_max_store(struct device *dev,
			      struct device_attribute *attr, char *buf,
			      size_t count)
{
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->in_max[16] = INS_TO_REG(16,
				      clamp_val(val, INT_MIN,
						INT_MAX - NEG12_OFFSET) +
				      NEG12_OFFSET);
	adm1026_write_value(client, ADM1026_REG_IN_MAX[16], data->in_max[16]);
	mutex_unlock(&data->update_lock);
	return count;
}

SENSOR_DEVICE_ATTR_RO(in16_input, in16, 16);
SENSOR_DEVICE_ATTR_RW(in16_min, in16_min, 16);
SENSOR_DEVICE_ATTR_RW(in16_max, in16_max, 16);

unsigned long fan_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", FAN_FROM_REG(data->fan[nr],
		data->fan_div[nr]));
}
unsigned long fan_min_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", FAN_FROM_REG(data->fan_min[nr],
		data->fan_div[nr]));
}
unsigned long fan_min_store(struct device *dev,
			     struct device_attribute *attr, char *buf,
			     size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->fan_min[nr] = FAN_TO_REG(val, data->fan_div[nr]);
	adm1026_write_value(client, ADM1026_REG_FAN_MIN(nr),
		data->fan_min[nr]);
	mutex_unlock(&data->update_lock);
	return count;
}

SENSOR_DEVICE_ATTR_RO(fan1_input, fan, 0);
SENSOR_DEVICE_ATTR_RW(fan1_min, fan_min, 0);
SENSOR_DEVICE_ATTR_RO(fan2_input, fan, 1);
SENSOR_DEVICE_ATTR_RW(fan2_min, fan_min, 1);
SENSOR_DEVICE_ATTR_RO(fan3_input, fan, 2);
SENSOR_DEVICE_ATTR_RW(fan3_min, fan_min, 2);
SENSOR_DEVICE_ATTR_RO(fan4_input, fan, 3);
SENSOR_DEVICE_ATTR_RW(fan4_min, fan_min, 3);
SENSOR_DEVICE_ATTR_RO(fan5_input, fan, 4);
SENSOR_DEVICE_ATTR_RW(fan5_min, fan_min, 4);
SENSOR_DEVICE_ATTR_RO(fan6_input, fan, 5);
SENSOR_DEVICE_ATTR_RW(fan6_min, fan_min, 5);
SENSOR_DEVICE_ATTR_RO(fan7_input, fan, 6);
SENSOR_DEVICE_ATTR_RW(fan7_min, fan_min, 6);
SENSOR_DEVICE_ATTR_RO(fan8_input, fan, 7);
SENSOR_DEVICE_ATTR_RW(fan8_min, fan_min, 7);

void fixup_fan_min(struct device *dev, int fan, int old_div)
{
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int new_min;
	int new_div = data->fan_div[fan];

	if (data->fan_min[fan] == 0 || data->fan_min[fan] == 0xff)
		return;

	new_min = data->fan_min[fan] * old_div / new_div;
	new_min = clamp_val(new_min, 1, 254);
	data->fan_min[fan] = new_min;
	adm1026_write_value(client, ADM1026_REG_FAN_MIN(fan), new_min);
}

unsigned long fan_div_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", data->fan_div[nr]);
}
unsigned long fan_div_store(struct device *dev,
			     struct device_attribute *attr, char *buf,
			     size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	long val;
	int orig_div, new_div;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	new_div = DIV_TO_REG(val);

	mutex_lock(&data->update_lock);
	orig_div = data->fan_div[nr];
	data->fan_div[nr] = DIV_FROM_REG(new_div);

	if (nr < 4) {
		adm1026_write_value(client, ADM1026_REG_FAN_DIV_0_3,
				    (DIV_TO_REG(data->fan_div[0]) << 0) |
				    (DIV_TO_REG(data->fan_div[1]) << 2) |
				    (DIV_TO_REG(data->fan_div[2]) << 4) |
				    (DIV_TO_REG(data->fan_div[3]) << 6));
	} else {
		adm1026_write_value(client, ADM1026_REG_FAN_DIV_4_7,
				    (DIV_TO_REG(data->fan_div[4]) << 0) |
				    (DIV_TO_REG(data->fan_div[5]) << 2) |
				    (DIV_TO_REG(data->fan_div[6]) << 4) |
				    (DIV_TO_REG(data->fan_div[7]) << 6));
	}

	if (data->fan_div[nr] != orig_div)
		fixup_fan_min(dev, nr, orig_div);

	mutex_unlock(&data->update_lock);
	return count;
}

SENSOR_DEVICE_ATTR_RW(fan1_div, fan_div, 0);
SENSOR_DEVICE_ATTR_RW(fan2_div, fan_div, 1);
SENSOR_DEVICE_ATTR_RW(fan3_div, fan_div, 2);
SENSOR_DEVICE_ATTR_RW(fan4_div, fan_div, 3);
SENSOR_DEVICE_ATTR_RW(fan5_div, fan_div, 4);
SENSOR_DEVICE_ATTR_RW(fan6_div, fan_div, 5);
SENSOR_DEVICE_ATTR_RW(fan7_div, fan_div, 6);
SENSOR_DEVICE_ATTR_RW(fan8_div, fan_div, 7);

unsigned long temp_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", TEMP_FROM_REG(data->temp[nr]));
}
unsigned long temp_min_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", TEMP_FROM_REG(data->temp_min[nr]));
}
unsigned long temp_min_store(struct device *dev,
			      struct device_attribute *attr, char *buf,
			      size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->temp_min[nr] = TEMP_TO_REG(val);
	adm1026_write_value(client, ADM1026_REG_TEMP_MIN[nr],
		data->temp_min[nr]);
	mutex_unlock(&data->update_lock);
	return count;
}
unsigned long temp_max_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", TEMP_FROM_REG(data->temp_max[nr]));
}
unsigned long temp_max_store(struct device *dev,
			      struct device_attribute *attr, char *buf,
			      size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->temp_max[nr] = TEMP_TO_REG(val);
	adm1026_write_value(client, ADM1026_REG_TEMP_MAX[nr],
		data->temp_max[nr]);
	mutex_unlock(&data->update_lock);
	return count;
}

SENSOR_DEVICE_ATTR_RO(temp1_input, temp, 0);
SENSOR_DEVICE_ATTR_RW(temp1_min, temp_min, 0);
SENSOR_DEVICE_ATTR_RW(temp1_max, temp_max, 0);
SENSOR_DEVICE_ATTR_RO(temp2_input, temp, 1);
SENSOR_DEVICE_ATTR_RW(temp2_min, temp_min, 1);
SENSOR_DEVICE_ATTR_RW(temp2_max, temp_max, 1);
SENSOR_DEVICE_ATTR_RO(temp3_input, temp, 2);
SENSOR_DEVICE_ATTR_RW(temp3_min, temp_min, 2);
SENSOR_DEVICE_ATTR_RW(temp3_max, temp_max, 2);

unsigned long temp_offset_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", TEMP_FROM_REG(data->temp_offset[nr]));
}
unsigned long temp_offset_store(struct device *dev,
				 struct device_attribute *attr,
				 char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->temp_offset[nr] = TEMP_TO_REG(val);
	adm1026_write_value(client, ADM1026_REG_TEMP_OFFSET[nr],
		data->temp_offset[nr]);
	mutex_unlock(&data->update_lock);
	return count;
}

SENSOR_DEVICE_ATTR_RW(temp1_offset, temp_offset, 0);
SENSOR_DEVICE_ATTR_RW(temp2_offset, temp_offset, 1);
SENSOR_DEVICE_ATTR_RW(temp3_offset, temp_offset, 2);

unsigned long temp_auto_point1_temp_hyst_show(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", TEMP_FROM_REG(
		ADM1026_FAN_ACTIVATION_TEMP_HYST + data->temp_tmin[nr]));
}
unsigned long temp_auto_point2_temp_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", TEMP_FROM_REG(data->temp_tmin[nr] +
		ADM1026_FAN_CONTROL_TEMP_RANGE));
}
unsigned long temp_auto_point1_temp_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", TEMP_FROM_REG(data->temp_tmin[nr]));
}
unsigned long temp_auto_point1_temp_store(struct device *dev,
					   struct device_attribute *attr,
					   char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->temp_tmin[nr] = TEMP_TO_REG(val);
	adm1026_write_value(client, ADM1026_REG_TEMP_TMIN[nr],
		data->temp_tmin[nr]);
	mutex_unlock(&data->update_lock);
	return count;
}

SENSOR_DEVICE_ATTR_RW(temp1_auto_point1_temp, temp_auto_point1_temp, 0);
SENSOR_DEVICE_ATTR_RO(temp1_auto_point1_temp_hyst,
			     temp_auto_point1_temp_hyst, 0);
SENSOR_DEVICE_ATTR_RO(temp1_auto_point2_temp, temp_auto_point2_temp, 0);
SENSOR_DEVICE_ATTR_RW(temp2_auto_point1_temp, temp_auto_point1_temp, 1);
SENSOR_DEVICE_ATTR_RO(temp2_auto_point1_temp_hyst,
			     temp_auto_point1_temp_hyst, 1);
SENSOR_DEVICE_ATTR_RO(temp2_auto_point2_temp, temp_auto_point2_temp, 1);
SENSOR_DEVICE_ATTR_RW(temp3_auto_point1_temp, temp_auto_point1_temp, 2);
SENSOR_DEVICE_ATTR_RO(temp3_auto_point1_temp_hyst,
			     temp_auto_point1_temp_hyst, 2);
SENSOR_DEVICE_ATTR_RO(temp3_auto_point2_temp, temp_auto_point2_temp, 2);

unsigned long show_temp_crit_enable(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", (data->config1 & CFG1_THERM_HOT) >> 4);
}
unsigned long set_temp_crit_enable(struct device *dev,
		struct device_attribute *attr, char *buf, size_t count)
{
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	if (val > 1)
		return -EINVAL;

	mutex_lock(&data->update_lock);
	data->config1 = (data->config1 & ~CFG1_THERM_HOT) | (val << 4);
	adm1026_write_value(client, ADM1026_REG_CONFIG1, data->config1);
	mutex_unlock(&data->update_lock);

	return count;
}

DEVICE_ATTR(temp1_crit_enable, 0644, show_temp_crit_enable,
		   set_temp_crit_enable);
DEVICE_ATTR(temp2_crit_enable, 0644, show_temp_crit_enable,
		   set_temp_crit_enable);
DEVICE_ATTR(temp3_crit_enable, 0644, show_temp_crit_enable,
		   set_temp_crit_enable);

unsigned long temp_crit_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", TEMP_FROM_REG(data->temp_crit[nr]));
}
unsigned long temp_crit_store(struct device *dev,
			       struct device_attribute *attr, char *buf,
			       size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->temp_crit[nr] = TEMP_TO_REG(val);
	adm1026_write_value(client, ADM1026_REG_TEMP_THERM[nr],
		data->temp_crit[nr]);
	mutex_unlock(&data->update_lock);
	return count;
}

SENSOR_DEVICE_ATTR_RW(temp1_crit, temp_crit, 0);
SENSOR_DEVICE_ATTR_RW(temp2_crit, temp_crit, 1);
SENSOR_DEVICE_ATTR_RW(temp3_crit, temp_crit, 2);

unsigned long analog_out_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", DAC_FROM_REG(data->analog_out));
}
unsigned long analog_out_store(struct device *dev,
				struct device_attribute *attr,
				char *buf, size_t count)
{
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->analog_out = DAC_TO_REG(val);
	adm1026_write_value(client, ADM1026_REG_DAC, data->analog_out);
	mutex_unlock(&data->update_lock);
	return count;
}

DEVICE_ATTR_RW(analog_out);

unsigned long cpu0_vid_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct adm1026_data *data = adm1026_update_device(dev);
	int vid = (data->gpio >> 11) & 0x1f;

	dev_dbg(dev, "Setting VID from GPIO11-15.\n");
	return sprintf(buf, "%d\n", vid_from_reg(vid, data->vrm));
}

DEVICE_ATTR_RO(cpu0_vid);

unsigned long vrm_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct adm1026_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", data->vrm);
}

unsigned long vrm_store(struct device *dev, struct device_attribute *attr,
			 char *buf, size_t count)
{
	struct adm1026_data *data = dev_get_drvdata(dev);
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	if (val > 255)
		return -EINVAL;

	data->vrm = val;
	return count;
}

DEVICE_ATTR_RW(vrm);

unsigned long alarms_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%ld\n", data->alarms);
}

DEVICE_ATTR_RO(alarms);

unsigned long alarm_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct adm1026_data *data = adm1026_update_device(dev);
	int bitnr = to_sensor_dev_attr(attr)->index;
	return sprintf(buf, "%ld\n", (data->alarms >> bitnr) & 1);
}

SENSOR_DEVICE_ATTR_RO(temp2_alarm, alarm, 0);
SENSOR_DEVICE_ATTR_RO(temp3_alarm, alarm, 1);
SENSOR_DEVICE_ATTR_RO(in9_alarm, alarm, 1);
SENSOR_DEVICE_ATTR_RO(in11_alarm, alarm, 2);
SENSOR_DEVICE_ATTR_RO(in12_alarm, alarm, 3);
SENSOR_DEVICE_ATTR_RO(in13_alarm, alarm, 4);
SENSOR_DEVICE_ATTR_RO(in14_alarm, alarm, 5);
SENSOR_DEVICE_ATTR_RO(in15_alarm, alarm, 6);
SENSOR_DEVICE_ATTR_RO(in16_alarm, alarm, 7);
SENSOR_DEVICE_ATTR_RO(in0_alarm, alarm, 8);
SENSOR_DEVICE_ATTR_RO(in1_alarm, alarm, 9);
SENSOR_DEVICE_ATTR_RO(in2_alarm, alarm, 10);
SENSOR_DEVICE_ATTR_RO(in3_alarm, alarm, 11);
SENSOR_DEVICE_ATTR_RO(in4_alarm, alarm, 12);
SENSOR_DEVICE_ATTR_RO(in5_alarm, alarm, 13);
SENSOR_DEVICE_ATTR_RO(in6_alarm, alarm, 14);
SENSOR_DEVICE_ATTR_RO(in7_alarm, alarm, 15);
SENSOR_DEVICE_ATTR_RO(fan1_alarm, alarm, 16);
SENSOR_DEVICE_ATTR_RO(fan2_alarm, alarm, 17);
SENSOR_DEVICE_ATTR_RO(fan3_alarm, alarm, 18);
SENSOR_DEVICE_ATTR_RO(fan4_alarm, alarm, 19);
SENSOR_DEVICE_ATTR_RO(fan5_alarm, alarm, 20);
SENSOR_DEVICE_ATTR_RO(fan6_alarm, alarm, 21);
SENSOR_DEVICE_ATTR_RO(fan7_alarm, alarm, 22);
SENSOR_DEVICE_ATTR_RO(fan8_alarm, alarm, 23);
SENSOR_DEVICE_ATTR_RO(temp1_alarm, alarm, 24);
SENSOR_DEVICE_ATTR_RO(in10_alarm, alarm, 25);
SENSOR_DEVICE_ATTR_RO(in8_alarm, alarm, 26);

unsigned long alarm_mask_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%ld\n", data->alarm_mask);
}
unsigned long alarm_mask_store(struct device *dev,
				struct device_attribute *attr,
				char *buf, size_t count)
{
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	unsigned long mask;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->alarm_mask = val & 0x7fffffff;
	mask = data->alarm_mask
		| (data->gpio_mask & 0x10000 ? 0x80000000 : 0);
	adm1026_write_value(client, ADM1026_REG_MASK1,
		mask & 0xff);
	mask >>= 8;
	adm1026_write_value(client, ADM1026_REG_MASK2,
		mask & 0xff);
	mask >>= 8;
	adm1026_write_value(client, ADM1026_REG_MASK3,
		mask & 0xff);
	mask >>= 8;
	adm1026_write_value(client, ADM1026_REG_MASK4,
		mask & 0xff);
	mutex_unlock(&data->update_lock);
	return count;
}

DEVICE_ATTR_RW(alarm_mask);

unsigned long gpio_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%ld\n", data->gpio);
}
unsigned long gpio_store(struct device *dev, struct device_attribute *attr,
			  char *buf, size_t count)
{
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	long gpio;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->gpio = val & 0x1ffff;
	gpio = data->gpio;
	adm1026_write_value(client, ADM1026_REG_GPIO_STATUS_0_7, gpio & 0xff);
	gpio >>= 8;
	adm1026_write_value(client, ADM1026_REG_GPIO_STATUS_8_15, gpio & 0xff);
	gpio = ((gpio >> 1) & 0x80) | (data->alarms >> 24 & 0x7f);
	adm1026_write_value(client, ADM1026_REG_STATUS4, gpio & 0xff);
	mutex_unlock(&data->update_lock);
	return count;
}

DEVICE_ATTR_RW(gpio);

unsigned long gpio_mask_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%ld\n", data->gpio_mask);
}
unsigned long gpio_mask_store(struct device *dev,
			       struct device_attribute *attr, char *buf,
			       size_t count)
{
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	long mask;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->gpio_mask = val & 0x1ffff;
	mask = data->gpio_mask;
	adm1026_write_value(client, ADM1026_REG_GPIO_MASK_0_7, mask & 0xff);
	mask >>= 8;
	adm1026_write_value(client, ADM1026_REG_GPIO_MASK_8_15, mask & 0xff);
	mask = ((mask >> 1) & 0x80) | (data->alarm_mask >> 24 & 0x7f);
	adm1026_write_value(client, ADM1026_REG_MASK1, mask & 0xff);
	mutex_unlock(&data->update_lock);
	return count;
}

DEVICE_ATTR_RW(gpio_mask);

unsigned long pwm1_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", PWM_FROM_REG(data->pwm1.pwm));
}

unsigned long pwm1_store(struct device *dev, struct device_attribute *attr,
			  char *buf, size_t count)
{
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	if (data->pwm1.enable == 1) {
		long val;
		int err;

		err = kstrtol(buf, 10, &val);
		if (err)
			return err;

		mutex_lock(&data->update_lock);
		data->pwm1.pwm = PWM_TO_REG(val);
		adm1026_write_value(client, ADM1026_REG_PWM, data->pwm1.pwm);
		mutex_unlock(&data->update_lock);
	}
	return count;
}

unsigned long temp1_auto_point1_pwm_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", data->pwm1.auto_pwm_min);
}

unsigned long temp1_auto_point1_pwm_store(struct device *dev,
					   struct device_attribute *attr,
					   char *buf, size_t count)
{
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->pwm1.auto_pwm_min = clamp_val(val, 0, 255);
	if (data->pwm1.enable == 2) {
		data->pwm1.pwm = PWM_TO_REG((data->pwm1.pwm & 0x0f) |
			PWM_MIN_TO_REG(data->pwm1.auto_pwm_min));
		adm1026_write_value(client, ADM1026_REG_PWM, data->pwm1.pwm);
	}
	mutex_unlock(&data->update_lock);
	return count;
}

unsigned long temp1_auto_point2_pwm_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	return sprintf(buf, "%d\n", ADM1026_PWM_MAX);
}

unsigned long pwm1_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct adm1026_data *data = adm1026_update_device(dev);
	return sprintf(buf, "%d\n", data->pwm1.enable);
}

unsigned long pwm1_enable_store(struct device *dev,
				 struct device_attribute *attr,
				 char *buf, size_t count)
{
	struct adm1026_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int old_enable;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	if (val >= 3)
		return -EINVAL;

	mutex_lock(&data->update_lock);
	old_enable = data->pwm1.enable;
	data->pwm1.enable = val;
	data->config1 = (data->config1 & ~CFG1_PWM_AFC)
			| ((val == 2) ? CFG1_PWM_AFC : 0);
	adm1026_write_value(client, ADM1026_REG_CONFIG1, data->config1);
	if (val == 2) {
		data->pwm1.pwm = PWM_TO_REG((data->pwm1.pwm & 0x0f) |
			PWM_MIN_TO_REG(data->pwm1.auto_pwm_min));
		adm1026_write_value(client, ADM1026_REG_PWM, data->pwm1.pwm);
	} else if (!((old_enable == 1) && (val == 1))) {
		data->pwm1.pwm = 255;
		adm1026_write_value(client, ADM1026_REG_PWM, data->pwm1.pwm);
	}
	mutex_unlock(&data->update_lock);

	return count;
}

DEVICE_ATTR_RW(pwm1);
DEVICE_ATTR(pwm2, 0644, pwm1_show, pwm1_store);
DEVICE_ATTR(pwm3, 0644, pwm1_show, pwm1_store);
DEVICE_ATTR_RW(pwm1_enable);
DEVICE_ATTR(pwm2_enable, 0644, pwm1_enable_show,
		   pwm1_enable_store);
DEVICE_ATTR(pwm3_enable, 0644, pwm1_enable_show,
		   pwm1_enable_store);
DEVICE_ATTR_RW(temp1_auto_point1_pwm);
DEVICE_ATTR(temp2_auto_point1_pwm, 0644,
		   temp1_auto_point1_pwm_show, temp1_auto_point1_pwm_store);
DEVICE_ATTR(temp3_auto_point1_pwm, 0644,
		   temp1_auto_point1_pwm_show, temp1_auto_point1_pwm_store);

DEVICE_ATTR_RO(temp1_auto_point2_pwm);
DEVICE_ATTR(temp2_auto_point2_pwm, 0444, temp1_auto_point2_pwm_show,
		   NULL);
DEVICE_ATTR(temp3_auto_point2_pwm, 0444, temp1_auto_point2_pwm_show,
		   NULL);

struct attribute **adm1026_attributes = {
	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in0_max.dev_attr.attr,
	&sensor_dev_attr_in0_min.dev_attr.attr,
	&sensor_dev_attr_in0_alarm.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in1_max.dev_attr.attr,
	&sensor_dev_attr_in1_min.dev_attr.attr,
	&sensor_dev_attr_in1_alarm.dev_attr.attr,
	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in2_max.dev_attr.attr,
	&sensor_dev_attr_in2_min.dev_attr.attr,
	&sensor_dev_attr_in2_alarm.dev_attr.attr,
	&sensor_dev_attr_in3_input.dev_attr.attr,
	&sensor_dev_attr_in3_max.dev_attr.attr,
	&sensor_dev_attr_in3_min.dev_attr.attr,
	&sensor_dev_attr_in3_alarm.dev_attr.attr,
	&sensor_dev_attr_in4_input.dev_attr.attr,
	&sensor_dev_attr_in4_max.dev_attr.attr,
	&sensor_dev_attr_in4_min.dev_attr.attr,
	&sensor_dev_attr_in4_alarm.dev_attr.attr,
	&sensor_dev_attr_in5_input.dev_attr.attr,
	&sensor_dev_attr_in5_max.dev_attr.attr,
	&sensor_dev_attr_in5_min.dev_attr.attr,
	&sensor_dev_attr_in5_alarm.dev_attr.attr,
	&sensor_dev_attr_in6_input.dev_attr.attr,
	&sensor_dev_attr_in6_max.dev_attr.attr,
	&sensor_dev_attr_in6_min.dev_attr.attr,
	&sensor_dev_attr_in6_alarm.dev_attr.attr,
	&sensor_dev_attr_in7_input.dev_attr.attr,
	&sensor_dev_attr_in7_max.dev_attr.attr,
	&sensor_dev_attr_in7_min.dev_attr.attr,
	&sensor_dev_attr_in7_alarm.dev_attr.attr,
	&sensor_dev_attr_in10_input.dev_attr.attr,
	&sensor_dev_attr_in10_max.dev_attr.attr,
	&sensor_dev_attr_in10_min.dev_attr.attr,
	&sensor_dev_attr_in10_alarm.dev_attr.attr,
	&sensor_dev_attr_in11_input.dev_attr.attr,
	&sensor_dev_attr_in11_max.dev_attr.attr,
	&sensor_dev_attr_in11_min.dev_attr.attr,
	&sensor_dev_attr_in11_alarm.dev_attr.attr,
	&sensor_dev_attr_in12_input.dev_attr.attr,
	&sensor_dev_attr_in12_max.dev_attr.attr,
	&sensor_dev_attr_in12_min.dev_attr.attr,
	&sensor_dev_attr_in12_alarm.dev_attr.attr,
	&sensor_dev_attr_in13_input.dev_attr.attr,
	&sensor_dev_attr_in13_max.dev_attr.attr,
	&sensor_dev_attr_in13_min.dev_attr.attr,
	&sensor_dev_attr_in13_alarm.dev_attr.attr,
	&sensor_dev_attr_in14_input.dev_attr.attr,
	&sensor_dev_attr_in14_max.dev_attr.attr,
	&sensor_dev_attr_in14_min.dev_attr.attr,
	&sensor_dev_attr_in14_alarm.dev_attr.attr,
	&sensor_dev_attr_in15_input.dev_attr.attr,
	&sensor_dev_attr_in15_max.dev_attr.attr,
	&sensor_dev_attr_in15_min.dev_attr.attr,
	&sensor_dev_attr_in15_alarm.dev_attr.attr,
	&sensor_dev_attr_in16_input.dev_attr.attr,
	&sensor_dev_attr_in16_max.dev_attr.attr,
	&sensor_dev_attr_in16_min.dev_attr.attr,
	&sensor_dev_attr_in16_alarm.dev_attr.attr,
	&sensor_dev_attr_fan1_input.dev_attr.attr,
	&sensor_dev_attr_fan1_div.dev_attr.attr,
	&sensor_dev_attr_fan1_min.dev_attr.attr,
	&sensor_dev_attr_fan1_alarm.dev_attr.attr,
	&sensor_dev_attr_fan2_input.dev_attr.attr,
	&sensor_dev_attr_fan2_div.dev_attr.attr,
	&sensor_dev_attr_fan2_min.dev_attr.attr,
	&sensor_dev_attr_fan2_alarm.dev_attr.attr,
	&sensor_dev_attr_fan3_input.dev_attr.attr,
	&sensor_dev_attr_fan3_div.dev_attr.attr,
	&sensor_dev_attr_fan3_min.dev_attr.attr,
	&sensor_dev_attr_fan3_alarm.dev_attr.attr,
	&sensor_dev_attr_fan4_input.dev_attr.attr,
	&sensor_dev_attr_fan4_div.dev_attr.attr,
	&sensor_dev_attr_fan4_min.dev_attr.attr,
	&sensor_dev_attr_fan4_alarm.dev_attr.attr,
	&sensor_dev_attr_fan5_input.dev_attr.attr,
	&sensor_dev_attr_fan5_div.dev_attr.attr,
	&sensor_dev_attr_fan5_min.dev_attr.attr,
	&sensor_dev_attr_fan5_alarm.dev_attr.attr,
	&sensor_dev_attr_fan6_input.dev_attr.attr,
	&sensor_dev_attr_fan6_div.dev_attr.attr,
	&sensor_dev_attr_fan6_min.dev_attr.attr,
	&sensor_dev_attr_fan6_alarm.dev_attr.attr,
	&sensor_dev_attr_fan7_input.dev_attr.attr,
	&sensor_dev_attr_fan7_div.dev_attr.attr,
	&sensor_dev_attr_fan7_min.dev_attr.attr,
	&sensor_dev_attr_fan7_alarm.dev_attr.attr,
	&sensor_dev_attr_fan8_input.dev_attr.attr,
	&sensor_dev_attr_fan8_div.dev_attr.attr,
	&sensor_dev_attr_fan8_min.dev_attr.attr,
	&sensor_dev_attr_fan8_alarm.dev_attr.attr,
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_max.dev_attr.attr,
	&sensor_dev_attr_temp1_min.dev_attr.attr,
	&sensor_dev_attr_temp1_alarm.dev_attr.attr,
	&sensor_dev_attr_temp2_input.dev_attr.attr,
	&sensor_dev_attr_temp2_max.dev_attr.attr,
	&sensor_dev_attr_temp2_min.dev_attr.attr,
	&sensor_dev_attr_temp2_alarm.dev_attr.attr,
	&sensor_dev_attr_temp1_offset.dev_attr.attr,
	&sensor_dev_attr_temp2_offset.dev_attr.attr,
	&sensor_dev_attr_temp1_auto_point1_temp.dev_attr.attr,
	&sensor_dev_attr_temp2_auto_point1_temp.dev_attr.attr,
	&sensor_dev_attr_temp1_auto_point1_temp_hyst.dev_attr.attr,
	&sensor_dev_attr_temp2_auto_point1_temp_hyst.dev_attr.attr,
	&sensor_dev_attr_temp1_auto_point2_temp.dev_attr.attr,
	&sensor_dev_attr_temp2_auto_point2_temp.dev_attr.attr,
	&sensor_dev_attr_temp1_crit.dev_attr.attr,
	&sensor_dev_attr_temp2_crit.dev_attr.attr,
	&dev_attr_temp1_crit_enable.attr,
	&dev_attr_temp2_crit_enable.attr,
	&dev_attr_cpu0_vid.attr,
	&dev_attr_vrm.attr,
	&dev_attr_alarms.attr,
	&dev_attr_alarm_mask.attr,
	&dev_attr_gpio.attr,
	&dev_attr_gpio_mask.attr,
	&dev_attr_pwm1.attr,
	&dev_attr_pwm2.attr,
	&dev_attr_pwm3.attr,
	&dev_attr_pwm1_enable.attr,
	&dev_attr_pwm2_enable.attr,
	&dev_attr_pwm3_enable.attr,
	&dev_attr_temp1_auto_point1_pwm.attr,
	&dev_attr_temp2_auto_point1_pwm.attr,
	&dev_attr_temp1_auto_point2_pwm.attr,
	&dev_attr_temp2_auto_point2_pwm.attr,
	&dev_attr_analog_out.attr,
	NULL
};

struct attribute **adm1026_attributes_temp3 = {
	&sensor_dev_attr_temp3_input.dev_attr.attr,
	&sensor_dev_attr_temp3_max.dev_attr.attr,
	&sensor_dev_attr_temp3_min.dev_attr.attr,
	&sensor_dev_attr_temp3_alarm.dev_attr.attr,
	&sensor_dev_attr_temp3_offset.dev_attr.attr,
	&sensor_dev_attr_temp3_auto_point1_temp.dev_attr.attr,
	&sensor_dev_attr_temp3_auto_point1_temp_hyst.dev_attr.attr,
	&sensor_dev_attr_temp3_auto_point2_temp.dev_attr.attr,
	&sensor_dev_attr_temp3_crit.dev_attr.attr,
	&dev_attr_temp3_crit_enable.attr,
	&dev_attr_temp3_auto_point1_pwm.attr,
	&dev_attr_temp3_auto_point2_pwm.attr,
	NULL
};

struct attribute **adm1026_attributes_in8_9 = {
	&sensor_dev_attr_in8_input.dev_attr.attr,
	&sensor_dev_attr_in8_max.dev_attr.attr,
	&sensor_dev_attr_in8_min.dev_attr.attr,
	&sensor_dev_attr_in8_alarm.dev_attr.attr,
	&sensor_dev_attr_in9_input.dev_attr.attr,
	&sensor_dev_attr_in9_max.dev_attr.attr,
	&sensor_dev_attr_in9_min.dev_attr.attr,
	&sensor_dev_attr_in9_alarm.dev_attr.attr,
	NULL
};

int adm1026_detect(struct i2c_client *client,
			  struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
	int address = client->addr;
	int company, verstep;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		return -ENODEV;
	}

	company = adm1026_read_value(client, ADM1026_REG_COMPANY);
	verstep = adm1026_read_value(client, ADM1026_REG_VERSTEP);

	dev_dbg(&adapter->dev,
		"Detecting device at %d,0x%02x with COMPANY: 0x%02x and VERSTEP: 0x%02x\n",
		i2c_adapter_id(client->adapter), client->addr,
		company, verstep);

	dev_dbg(&adapter->dev, "Autodetecting device at %d,0x%02x...\n",
		i2c_adapter_id(adapter), address);
	if (company == ADM1026_COMPANY_ANALOG_DEV
	    && verstep == ADM1026_VERSTEP_ADM1026) {
	} else if (company == ADM1026_COMPANY_ANALOG_DEV
		&& (verstep & 0xf0) == ADM1026_VERSTEP_GENERIC) {
		dev_err(&adapter->dev,
			"Unrecognized stepping 0x%02x. Defaulting to ADM1026.\n",
			verstep);
	} else if ((verstep & 0xf0) == ADM1026_VERSTEP_GENERIC) {
		dev_err(&adapter->dev,
			"Found version/stepping 0x%02x. Assuming generic ADM1026.\n",
			verstep);
	} else {
		dev_dbg(&adapter->dev, "Autodetection failed\n");
		return -ENODEV;
	}

	strscpy(info->type, "adm1026", I2C_NAME_SIZE);

	return 0;
}

void adm1026_print_gpio(struct i2c_client *client)
{
	struct adm1026_data *data = i2c_get_clientdata(client);
	int i;

	dev_dbg(&client->dev, "GPIO config is:\n");
	for (i = 0; i <= 7; ++i) {
		if (data->config2 & (1 << i)) {
			dev_dbg(&client->dev, "\t%sGP%s%d\n",
				data->gpio_config[i] & 0x02 ? "" : "!",
				data->gpio_config[i] & 0x01 ? "OUT" : "IN",
				i);
		} else {
			dev_dbg(&client->dev, "\tFAN%d\n", i);
		}
	}
	for (i = 8; i <= 15; ++i) {
		dev_dbg(&client->dev, "\t%sGP%s%d\n",
			data->gpio_config[i] & 0x02 ? "" : "!",
			data->gpio_config[i] & 0x01 ? "OUT" : "IN",
			i);
	}
	if (data->config3 & CFG3_GPIO16_ENABLE) {
		dev_dbg(&client->dev, "\t%sGP%s16\n",
			data->gpio_config[16] & 0x02 ? "" : "!",
			data->gpio_config[16] & 0x01 ? "OUT" : "IN");
	} else {
		dev_dbg(&client->dev, "\tTHERM\n");
	}
}

void adm1026_fixup_gpio(struct i2c_client *client)
{
	struct adm1026_data *data = i2c_get_clientdata(client);
	int i;
	int value;

	for (i = 0; i <= 16; ++i) {
		if (gpio_output[i] >= 0 && gpio_output[i] <= 16)
			data->gpio_config[gpio_output[i]] |= 0x01;
		if (gpio_output[i] >= 0 && gpio_output[i] <= 7)
			data->config2 |= 1 << gpio_output[i];
	}

	for (i = 0; i <= 16; ++i) {
		if (gpio_input[i] >= 0 && gpio_input[i] <= 16)
			data->gpio_config[gpio_input[i]] &= ~0x01;
		if (gpio_input[i] >= 0 && gpio_input[i] <= 7)
			data->config2 |= 1 << gpio_input[i];
	}

	for (i = 0; i <= 16; ++i) {
		if (gpio_inverted[i] >= 0 && gpio_inverted[i] <= 16)
			data->gpio_config[gpio_inverted[i]] &= ~0x02;
	}

	for (i = 0; i <= 16; ++i) {
		if (gpio_normal[i] >= 0 && gpio_normal[i] <= 16)
			data->gpio_config[gpio_normal[i]] |= 0x02;
	}

	for (i = 0; i <= 7; ++i) {
		if (gpio_fan[i] >= 0 && gpio_fan[i] <= 7)
			data->config2 &= ~(1 << gpio_fan[i]);
	}

	adm1026_write_value(client, ADM1026_REG_CONFIG2, data->config2);
	data->config3 = (data->config3 & 0x3f)
			| ((data->gpio_config[16] & 0x03) << 6);
	adm1026_write_value(client, ADM1026_REG_CONFIG3, data->config3);
	value = 0;
	for (i = 15; i >= 0; --i) {
		value <<= 2;
		value |= data->gpio_config[i] & 0x03;
		if ((i & 0x03) == 0) {
			adm1026_write_value(client,
					ADM1026_REG_GPIO_CFG_0_3 + i/4,
					value);
			value = 0;
		}
	}

	adm1026_print_gpio(client);
}

void adm1026_init_client(struct i2c_client *client)
{
	int value, i;
	struct adm1026_data *data = i2c_get_clientdata(client);

	dev_dbg(&client->dev, "Initializing device\n");
	data->config1 = adm1026_read_value(client, ADM1026_REG_CONFIG1);
	data->config2 = adm1026_read_value(client, ADM1026_REG_CONFIG2);
	data->config3 = adm1026_read_value(client, ADM1026_REG_CONFIG3);

	dev_dbg(&client->dev, "ADM1026_REG_CONFIG1 is: 0x%02x\n",
		data->config1);
	if ((data->config1 & CFG1_MONITOR) == 0) {
		dev_dbg(&client->dev,
			"Monitoring not currently enabled.\n");
	}
	if (data->config1 & CFG1_INT_ENABLE) {
		dev_dbg(&client->dev,
			"SMBALERT interrupts are enabled.\n");
	}
	if (data->config1 & CFG1_AIN8_9) {
		dev_dbg(&client->dev,
			"in8 and in9 enabled. temp3 disabled.\n");
	} else {
		dev_dbg(&client->dev,
			"temp3 enabled.  in8 and in9 disabled.\n");
	}
	if (data->config1 & CFG1_THERM_HOT) {
		dev_dbg(&client->dev,
			"Automatic THERM, PWM, and temp limits enabled.\n");
	}

	if (data->config3 & CFG3_GPIO16_ENABLE) {
		dev_dbg(&client->dev,
			"GPIO16 enabled.  THERM pin disabled.\n");
	} else {
		dev_dbg(&client->dev,
			"THERM pin enabled.  GPIO16 disabled.\n");
	}
	if (data->config3 & CFG3_VREF_250)
		dev_dbg(&client->dev, "Vref is 2.50 Volts.\n");
	else
		dev_dbg(&client->dev, "Vref is 1.82 Volts.\n");
	value = 0;
	for (i = 0; i <= 15; ++i) {
		if ((i & 0x03) == 0) {
			value = adm1026_read_value(client,
					ADM1026_REG_GPIO_CFG_0_3 + i / 4);
		}
		data->gpio_config[i] = value & 0x03;
		value >>= 2;
	}
	data->gpio_config[16] = (data->config3 >> 6) & 0x03;

	adm1026_print_gpio(client);

	if (gpio_input[0] != -1 || gpio_output[0] != -1
		|| gpio_inverted[0] != -1 || gpio_normal[0] != -1
		|| gpio_fan[0] != -1) {
		adm1026_fixup_gpio(client);
	}

	data->pwm1.auto_pwm_min = 255;
	value = adm1026_read_value(client, ADM1026_REG_CONFIG1);
	value = (value | CFG1_MONITOR) & (~CFG1_INT_CLEAR & ~CFG1_RESET);
	dev_dbg(&client->dev, "Setting CONFIG to: 0x%02x\n", value);
	data->config1 = value;
	adm1026_write_value(client, ADM1026_REG_CONFIG1, value);

	value = adm1026_read_value(client, ADM1026_REG_FAN_DIV_0_3) |
		(adm1026_read_value(client, ADM1026_REG_FAN_DIV_4_7) << 8);
	for (i = 0; i <= 7; ++i) {
		data->fan_div[i] = DIV_FROM_REG(value & 0x03);
		value >>= 2;
	}
}

int adm1026_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct device *hwmon_dev;
	struct adm1026_data *data;

	data = devm_kzalloc(dev, sizeof(struct adm1026_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	i2c_set_clientdata(client, data);
	data->client = client;
	mutex_init(&data->update_lock);

	data->vrm = vid_which_vrm();

	adm1026_init_client(client);

	data->groups[0] = &adm1026_group;
	if (data->config1 & CFG1_AIN8_9)
		data->groups[1] = &adm1026_group_in8_9;
	else
		data->groups[1] = &adm1026_group_temp3;

	hwmon_dev = devm_hwmon_device_register_with_groups(dev, client->name,
							   data, data->groups);
	return PTR_ERR_OR_ZERO(hwmon_dev);
}

struct i2c_device_id* adm1026_id = {
	{ "adm1026" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, adm1026_id);

module_i2c_driver(adm1026_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Philip Pokorny <ppokorny@penguincomputing.com>, "
	      "Justin Thiessen <jthiessen@penguincomputing.com>");
MODULE_DESCRIPTION("ADM1026 driver");
