#include "wch_usbhid_internal.h"
#include <string.h>

volatile uint8_t  USB_SetupReq, USB_SetupTyp, USB_Config, USB_Addr, USB_ENUM_OK;
volatile uint16_t USB_SetupLen;
const uint8_t*    USB_pDescr;

// HID State
volatile uint8_t hidIdleRate = 0;
volatile uint8_t hidProtocol = 1;
volatile uint8_t wch_hid_ep1_complete = 1; // Sync flag
volatile uint8_t wch_hid_ep2_complete = 1;

static volatile uint8_t feature_report_available = 0;
static volatile uint8_t feature_report_len = 0;
static uint8_t feature_report_buffer[64];
static uint8_t feature_response_buffer[64];
static uint8_t feature_response_len = 0;

static uint8_t is_vendor_feature_report(void) {
  return USB_SetupBuf->wValueH == 0x03 && USB_SetupBuf->wValueL == 0x03;
}

// Internal Init (Using EP1 - Standard Keyboard)
static inline void USB_EP_init(void) {
  USBFSD->UEP0_DMA    = (uint32_t)wch_usbhid_EP0_buffer;
  USBFSD->UEP0_CTRL_H = USBFS_UEP_R_RES_ACK | USBFS_UEP_T_RES_NAK;
  USBFSD->UEP0_TX_LEN = 0;

  // EP1 used for IN (Keyboard)
  USBFSD->UEP1_DMA    = (uint32_t)wch_usbhid_EP1_buffer;
  USBFSD->UEP2_DMA    = (uint32_t)wch_usbhid_EP2_buffer;
  USBFSD->UEP4_1_MOD  = USBFS_UEP1_TX_EN; 
  USBFSD->UEP2_3_MOD  = USBFS_UEP2_TX_EN;
  USBFSD->UEP1_CTRL_H = USBFS_UEP_AUTO_TOG | USBFS_UEP_T_RES_NAK; 
  USBFSD->UEP2_CTRL_H = USBFS_UEP_AUTO_TOG | USBFS_UEP_T_RES_NAK;
  USBFSD->UEP1_TX_LEN = 0;
  USBFSD->UEP2_TX_LEN = 0;
  
  wch_hid_ep1_complete = 1; // Ready
  wch_hid_ep2_complete = 1;

  USB_ENUM_OK = 0;
  USB_Config  = 0;
  USB_Addr    = 0;
}

void USB_init(void) {
    // 1. Enable Clocks
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBFS, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE); // Power Clock for voltage check
    
    // Configure Flash Latency for 48MHz
    volatile uint32_t* flash_actlr = (volatile uint32_t*)0x40022000;
    *flash_actlr = (*flash_actlr & ~0x03) | 0x02; 
    
    // USBFS Reset Sequence
    // Disable PHY/Pull-ups
    #ifdef UDP_PUE_MASK
        AFIO->CTLR &= ~(UDP_PUE_MASK | UDM_PUE_MASK | USB_IOEN | USB_PHY_V33);
    #else
        AFIO->CTLR &= ~(AFIO_CTLR_UDP_PUE | AFIO_CTLR_UDM_PUE | AFIO_CTLR_USB_IOEN | AFIO_CTLR_USB_PHY_V33);
    #endif
    
    // Reset SIE
    USBFSD->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
    for(volatile int i = 0; i < 50000; i++) __NOP();
    USBFSD->BASE_CTRL = 0x00;
    
    // 3. Configure PHY (Assuming 3.3V based on PWR check)
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_16;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_17;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    
    // Check Voltage (If PVDO set, Voltage High -> 3.3V)
    uint32_t afio = AFIO->CTLR;
    // For standard dev boards, force 3.3V config
    #ifdef UDP_PUE_MASK
        afio |= (USB_PHY_V33 | UDP_PUE_1K5 | USB_IOEN);
    #else
        afio |= (AFIO_CTLR_USB_PHY_V33 | AFIO_CTLR_UDP_PUE_1K5 | AFIO_CTLR_USB_IOEN);
    #endif
    AFIO->CTLR = afio;
    
    for(volatile int i = 0; i < 20000; i++) __NOP();

    generate_all_string_descriptors();

    // 4. Init Core & Buffers
    memset(wch_usbhid_EP0_buffer, 0, EP0_SIZE + 2);
    memset(wch_usbhid_EP1_buffer, 0, EP1_SIZE + 2);
    memset(wch_usbhid_EP2_buffer, 0, EP1_SIZE + 2);
    
    USBFSD->UEP4_1_MOD = 0;
    USBFSD->UEP2_3_MOD = 0;
    
    USB_EP_init();
    
    USBFSD->DEV_ADDR = 0x00;
    USBFSD->INT_FG = 0xff;
    
    USBFSD->UDEV_CTRL = USBFS_UD_PD_DIS | USBFS_UD_PORT_EN;
    USBFSD->BASE_CTRL = USBFS_UC_DEV_PU_EN | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
    
    for(volatile int i = 0; i < 100000; i++) __NOP();

    USBFSD->INT_EN = USBFS_UIE_SUSPEND | USBFS_UIE_BUS_RST | USBFS_UIE_TRANSFER;
    NVIC_EnableIRQ(USBFS_IRQn);
}

void USB_EP0_copyDescr(uint8_t len) {
  uint8_t* tgt = wch_usbhid_EP0_buffer;
  while(len--) *tgt++ = *USB_pDescr++;
}

static inline void USB_EP0_SETUP(void) {
  uint8_t len = 0;
  USB_SetupLen = ((uint16_t)USB_SetupBuf->wLengthH<<8) | (USB_SetupBuf->wLengthL);
  USB_SetupReq = USB_SetupBuf->bRequest;
  USB_SetupTyp = USB_SetupBuf->bRequestType;

  if((USB_SetupTyp & 0x60) == 0x00) {
    switch(USB_SetupReq) {
      case 0x06: 
        switch(USB_SetupBuf->wValueH) {
          case USB_DESCR_TYP_DEVICE:
            USB_pDescr = (const uint8_t*)&wch_usbhid_DevDescr; len = sizeof(wch_usbhid_DevDescr); break;
          case USB_DESCR_TYP_CONFIG:
            USB_pDescr = wch_usbhid_CfgDescr; len = wch_usbhid_CfgDescrLen; break;
          case USB_DESCR_TYP_STRING: {
            switch(USB_SetupBuf->wValueL) {
              case 0:   USB_pDescr = (const uint8_t*)&wch_usbhid_LangDescr; break;
              case 1:   USB_pDescr = (const uint8_t*)&wch_usbhid_ManufDescr; break;
              case 2:   USB_pDescr = (const uint8_t*)&wch_usbhid_ProdDescr; break;
              case 3:   USB_pDescr = (const uint8_t*)&wch_usbhid_SerDescr; break;
              case 4:   USB_pDescr = (const uint8_t*)&wch_usbhid_InterfDescr; break;
              default:  USB_pDescr = (const uint8_t*)&wch_usbhid_SerDescr; break;
            }
            len = ((const uint8_t*)USB_pDescr)[0];
            break;
          }
          case 0x22: // HID Report Descriptor
            if (USB_SetupBuf->wIndexL == 1) {
              USB_pDescr = wch_usbhid_VendorReportDescr;
              len = wch_usbhid_VendorReportDescrLen;
            } else {
              USB_pDescr = wch_usbhid_KeyboardReportDescr;
              len = wch_usbhid_KeyboardReportDescrLen;
            }
            break;
          case 0x21: // HID Descriptor
            USB_pDescr = USB_SetupBuf->wIndexL == 1 ? &wch_usbhid_CfgDescr[43] : &wch_usbhid_CfgDescr[18];
            len = 9;
            break;
          default: len = 0xff; break;
        }
        if(len != 0xff) {
          if(USB_SetupLen > len) USB_SetupLen = len;
          len = USB_SetupLen >= EP0_SIZE ? EP0_SIZE : USB_SetupLen;
          USB_EP0_copyDescr(len);
        }
        break;
      case 0x05: // SET_ADDRESS
        USB_Addr = USB_SetupBuf->wValueL; break;
      case 0x08: // GET_CONFIGURATION
        wch_usbhid_EP0_buffer[0] = USB_Config; if(USB_SetupLen > 1) USB_SetupLen = 1; len = USB_SetupLen; break;
      case 0x09: // SET_CONFIGURATION
        USB_Config  = USB_SetupBuf->wValueL; USB_ENUM_OK = 1; break;
      case 0x00: // GET_STATUS
        wch_usbhid_EP0_buffer[0] = 0x00; wch_usbhid_EP0_buffer[1] = 0x00; if(USB_SetupLen > 2) USB_SetupLen = 2; len = USB_SetupLen; break;
      default:
        len = 0xff; break;
    }
  } 
  else if ((USB_SetupTyp & 0x60) == 0x20) {
      switch(USB_SetupReq) {
          case HID_REQ_GET_REPORT:
              if (is_vendor_feature_report()) {
                  len = USB_SetupLen;
                  if (len > sizeof(feature_response_buffer)) len = sizeof(feature_response_buffer);
                  if (len > feature_response_len) len = feature_response_len;
                  memcpy(wch_usbhid_EP0_buffer, feature_response_buffer, len);
              } else {
                  len = 0xff;
              }
              break;
          case HID_REQ_SET_IDLE:
              hidIdleRate = USB_SetupBuf->wValueH;
              len = 0; 
              break;
          case HID_REQ_GET_IDLE:
              wch_usbhid_EP0_buffer[0] = hidIdleRate;
              len = 1;
              break;
          case HID_REQ_SET_PROTO:
              hidProtocol = USB_SetupBuf->wValueL;
              len = 0;
              break;
          case HID_REQ_GET_PROTO:
              wch_usbhid_EP0_buffer[0] = hidProtocol;
              len = 1;
              break;
          case HID_REQ_SET_REPORT:
              if (is_vendor_feature_report()) {
                  len = USB_SetupLen;
                  if (len > EP0_SIZE) len = EP0_SIZE;
              } else {
                  len = USB_SetupLen; 
                  if (len > EP0_SIZE) len = EP0_SIZE;
              }
              break;
          default:
              len = 0xff;
              break;
      }
  }
  else {
    len = 0xff;
  }

  if(len == 0xff) {
    USB_SetupReq = 0xff;
    USBFSD->UEP0_CTRL_H = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_STALL | USBFS_UEP_R_TOG | USBFS_UEP_R_RES_STALL;
  } else {
    USB_SetupLen -= len;
    USBFSD->UEP0_TX_LEN = len;
    USBFSD->UEP0_CTRL_H = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_ACK | USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK;
  }
}

static inline void USB_EP0_IN(void) {
  uint8_t len;
  if((USB_SetupTyp & 0x60) == 0x20) return; // HID Fix

  switch(USB_SetupReq) {
    case 0x06: 
      len = USB_SetupLen >= EP0_SIZE ? EP0_SIZE : USB_SetupLen;
      USB_EP0_copyDescr(len);
      USB_SetupLen -= len;
      USBFSD->UEP0_TX_LEN = len;
      USBFSD->UEP0_CTRL_H ^= USBFS_UEP_T_TOG;
      break;
    case 0x05: 
      USBFSD->DEV_ADDR    = (USBFSD->DEV_ADDR & USBFS_UDA_GP_BIT) | USB_Addr;
      USBFSD->UEP0_CTRL_H = USBFS_UEP_T_RES_NAK | USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK;
      break;
    default:
      USBFSD->UEP0_CTRL_H = USBFS_UEP_T_RES_NAK | USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK;
      break;
  }
}

static inline void USB_EP0_OUT(void) {
  if (USB_SetupReq == HID_REQ_SET_REPORT) {
      if (USB_SetupTyp & 0x20) {
          uint8_t len = USBFSD->RX_LEN;
          if (len > sizeof(feature_report_buffer)) len = sizeof(feature_report_buffer);
          memcpy(feature_report_buffer, wch_usbhid_EP0_buffer, len);
          feature_report_len = len;
          feature_report_available = 1;
          if (USB_SetupLen >= len) {
              USB_SetupLen -= len;
          } else {
              USB_SetupLen = 0;
          }
      }
      USBFSD->UEP0_TX_LEN = 0;
      USBFSD->UEP0_CTRL_H = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_ACK | USBFS_UEP_R_RES_ACK;
      USB_SetupReq = 0; 
  } else {
      USBFSD->UEP0_CTRL_H = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_ACK | USBFS_UEP_R_RES_ACK;
  }
}

// Write to Endpoint 1 using synchronized polling
uint32_t USB_write(const uint8_t* buf, uint32_t len) {
    if(!USB_ENUM_OK) return 0;
    if(len > 64) len = 64;
    
    // Wait for buffer to be free
    uint32_t timeout = 5000000;
    while(!wch_hid_ep1_complete) {
        if(--timeout == 0) break;
        if(!USB_ENUM_OK) return 0;
    }
    
    wch_hid_ep1_complete = 0; // Mark busy
    
    // Copy data to EP1 buffer
    memcpy(wch_usbhid_EP1_buffer, buf, len);
    USBFSD->UEP1_TX_LEN = len;
    
    // Set ACK to transmit
    USBFSD->UEP1_CTRL_H = (USBFSD->UEP1_CTRL_H & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
    
    // Sync Wait: Ensure transfer completes before returning
    timeout = 5000000;
    while(!wch_hid_ep1_complete) {
        if(--timeout == 0) {
            wch_hid_ep1_complete = 1; // Force ready on timeout
            break; 
        }
        if(!USB_ENUM_OK) return 0;
    }
    
    return len;
}

uint32_t USB_writeVendorInputReport(const uint8_t* buf, uint32_t len) {
    if(!USB_ENUM_OK) return 0;
    if(len > 63) len = 63;

    uint32_t timeout = 5000000;
    while(!wch_hid_ep2_complete) {
        if(--timeout == 0) break;
        if(!USB_ENUM_OK) return 0;
    }

    wch_hid_ep2_complete = 0;
    memset(wch_usbhid_EP2_buffer, 0, EP1_SIZE);
    wch_usbhid_EP2_buffer[0] = 0x03;
    memcpy(wch_usbhid_EP2_buffer + 1, buf, len);
    USBFSD->UEP2_TX_LEN = EP1_SIZE;
    USBFSD->UEP2_CTRL_H = (USBFSD->UEP2_CTRL_H & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;

    timeout = 5000000;
    while(!wch_hid_ep2_complete) {
        if(--timeout == 0) {
            wch_hid_ep2_complete = 1;
            USBFSD->UEP2_CTRL_H = (USBFSD->UEP2_CTRL_H & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
            break;
        }
        if(!USB_ENUM_OK) return 0;
    }

    return len;
}

uint8_t USB_ready(void) {
    return USB_ENUM_OK;
}

uint8_t USB_featureReportAvailable(void) {
    return feature_report_available;
}

uint8_t USB_readFeatureReport(uint8_t* buffer, uint8_t max_len) {
    if (!feature_report_available) return 0;
    uint8_t len = feature_report_len;
    if (len > max_len) len = max_len;
    memcpy(buffer, feature_report_buffer, len);
    feature_report_available = 0;
    feature_report_len = 0;
    return len;
}

void USB_setFeatureReportResponse(const uint8_t* buffer, uint8_t len) {
    if (len > sizeof(feature_response_buffer)) len = sizeof(feature_response_buffer);
    memcpy(feature_response_buffer, buffer, len);
    feature_response_len = len;
}

void USBFS_IRQHandler(void) __attribute__((interrupt));
void USBFS_IRQHandler(void) {
  uint8_t intflag = USBFSD->INT_FG;
  uint8_t intst   = USBFSD->INT_ST;
  if(intflag & USBFS_UIF_TRANSFER) {
    uint8_t callIndex = intst & USBFS_UIS_ENDP_MASK;
    switch(intst & USBFS_UIS_TOKEN_MASK) {
      case USBFS_UIS_TOKEN_SETUP: USB_EP0_SETUP(); break;
      case USBFS_UIS_TOKEN_IN:
        switch(callIndex) { 
            case 0: USB_EP0_IN(); break; 
            case 1: wch_hid_ep1_complete = 1; break; // Signal EP1 Done
            case 2:
              wch_hid_ep2_complete = 1;
              USBFSD->UEP2_CTRL_H = (USBFSD->UEP2_CTRL_H & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
              break;
            default: break; 
        }
        break;
      case USBFS_UIS_TOKEN_OUT:
        switch(callIndex) { case 0: USB_EP0_OUT(); break; default: break; }
        break;
    }
    USBFSD->INT_FG = USBFS_UIF_TRANSFER;
  }
  if(intflag & USBFS_UIF_SUSPEND) { USBFSD->INT_FG = USBFS_UIF_SUSPEND; }
  if(intflag & USBFS_UIF_BUS_RST) {
    USB_EP_init();
    USBFSD->DEV_ADDR = 0; USBFSD->INT_FG = 0xff;
  }
}
