/* @file  main.c
   @brief this code involves NRF52 as a BLE Beacon and includes the beacon 
   advertising data for iBeacon, Eddystone and Custom format.
   @author Avinashee Tech
*/

#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/drivers/uart.h>  //for uart
#include <zephyr/sys/util.h>

//for bluetooth
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>

#define iBEACON       0
#define EDDYSTONE_URL 0
#define EDDYSTONE_UID 0


#ifndef BEACON_RSSI
#define BEACON_RSSI 0xC8
#endif

#ifndef BEACON_UUID
#define BEACON_UUID       0xae,0xf2,0x46,0x5c,0x78,0xc0,0x47,0x44,0xb7,0x2e,0xd6,0xaa,0xf3,0x0d,0x1d,0x76
#endif

#define UART_DEBUG        0    //enable to log uart messages
char ble_uart_buffer[150] = {0};
//uart configuration
const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));
const struct uart_config uart_cfg = {
		.baudrate = 115200,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE
};

static K_SEM_DEFINE(ble_init_ok,0,1);   //senaphore for ble init

/*
 *@brief : uart tranmsit function
 *@param : buffer to be transmitted
 *@retval : None 
 *@note : function to transmit uart messages using blocking poll out method
*/
void nrf52_uart_tx(uint8_t *tx_buff){
	for(int buflen=0;buflen<=strlen(tx_buff);buflen++){
                uart_poll_out(uart_dev,*(tx_buff+buflen));
	}
	k_msleep(100);
}

/*
 * Set beacon advertisement data according to format chosen.
 *
 * For iBeacon and Eddystone
 * UUID:  18ee1516-016b-4bec-ad96-bcb96d166e97
 * Major: 0
 * Minor: 0
 * RSSI:  -56 dBm
 * 
 * For custom
 * UUID: 761d0df3-aad6-2eb7-4447-c0785c46f2ae
 * 
 */
static const struct bt_data ad[] = {
#if iBEACON
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA,
		      0x4c, 0x00, /* Apple */
		      0x02, 0x15, /* iBeacon */
		      0x18, 0xee, 0x15, 0x16, /* UUID[15..12] */
		      0x01, 0x6b, /* UUID[11..10] */
		      0x4b, 0xec, /* UUID[9..8] */
		      0xad, 0x96, /* UUID[7..6] */
		      0xbc, 0xb9, 0x6d, 0x16, 0x6e, 0x97, /* UUID[5..0] */
		      0x00, 0x00, /* Major */
		      0x00, 0x00, /* Minor */
		      BEACON_RSSI) /* Calibrated RSSI @ 1m */
#elif EDDYSTONE_URL	
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xaa, 0xfe),
	BT_DATA_BYTES(BT_DATA_SVC_DATA16,
		      0xaa, 0xfe, /* Eddystone UUID */
		      0x10, /* Eddystone-URL frame type */
		      0x00, /* Calibrated Tx power at 0m */
		      0x00, /* URL Scheme Prefix http://www. */
			  'r','b','.','g','y','/','3','f','m','4','g','2')
#elif EDDYSTONE_UID
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xaa, 0xfe),
	BT_DATA_BYTES(BT_DATA_SVC_DATA16,
		      0xaa, 0xfe, /* Eddystone UUID */
			  0x00,
			  BEACON_RSSI,
			  0x41,0x56,0x49,0x4E,0x41,0x53,0x48,0x45,0x45,0x20, /*Namespace -Avinashee */
			  0x4E,0x52,0x46,0x30,0x30,0x31 /*Instance-NRF001*/)
#else 
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL,BEACON_UUID)
#endif
};

/*
 *@brief : Callback for notifying that Bluetooth has been enabled.
 *@param : error condition (zero on success or (negative) error code otherwise.)
 *@retval : None 
*/
static void bt_ready(int err)
{
	char addr_s[BT_ADDR_LE_STR_LEN];
	bt_addr_le_t addr = {0};
	size_t count = 1;
	

	if (err) 
	{
#if UART_DEBUG
		memset(ble_uart_buffer,0,sizeof(ble_uart_buffer));
		sprintf(ble_uart_buffer,"BLE init failed with error code %d\n", err);
		nrf52_uart_tx(ble_uart_buffer);
#endif
		return;
	}

	/* Start advertising */
	err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad),
			      NULL, 0);

	if (err) 
	{
#if UART_DEBUG
		memset(ble_uart_buffer,0,sizeof(ble_uart_buffer));
		sprintf(ble_uart_buffer,"Advertising failed to start (err %d)\n", err);
		nrf52_uart_tx(ble_uart_buffer);
#endif
		return;
	}

	nrf52_uart_tx("Advertising started!\n");


	/* For non-connectable non-identity
	 * advertising an non-resolvable private address is used;
	 * there is no API to retrieve that.
	 */

	bt_id_get(&addr, &count);
	bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));

#if UART_DEBUG
	memset(ble_uart_buffer,0,sizeof(ble_uart_buffer));
	sprintf(ble_uart_buffer,"Beacon started, advertising as %s\n",addr_s);
	nrf52_uart_tx(ble_uart_buffer);
#endif

    k_sem_give(&ble_init_ok);
}

/*
 *@brief : main application
 *@param : None
 *@retval : error status, if any 
*/
int main(void)
{
	int err;

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(bt_ready);
	if (err) {
#if UART_DEBUG
		memset(ble_uart_buffer,0,sizeof(ble_uart_buffer));
		sprintf(ble_uart_buffer,"Bluetooth init failed (err %d)\n", err);
		nrf52_uart_tx(ble_uart_buffer);
#endif		
	}

	err = k_sem_take(&ble_init_ok,K_MSEC(500));
	if (!err) 
	{
#if UART_DEBUG
		//Bluetooth initialized
		nrf52_uart_tx("bluetooth init success\n");
#endif
	} else 
	{
#if UART_DEBUG
                //Bluetooth initialization did not complete in Time
		nrf52_uart_tx("bluetooth init timeout\n");
#endif	
	}
	return 0;
}
