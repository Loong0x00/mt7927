# Router Configuration Analysis — CMCC GS2107

**Date**: 2026-02-17
**Router IP**: 192.168.1.1
**Model**: GS2107 (CMCC-branded, Boa/0.94.13 web server)
**Hardware**: HG530RRA.VER.C
**Software**: V100R001C01B010
**WiFi Chip**: MT7663 (RTDEVICE=7663)

---

## 5GHz WiFi Configuration (CMCC-Pg2Y-5G-FAST) — Target AP

| Setting | Value | Notes |
|---------|-------|-------|
| **Enabled** | Yes | AP is active |
| **SSID** | CMCC-Pg2Y-5G-FAST | Primary SSID (SSID5/index 0) |
| **SSID Hidden** | No | Broadcasting normally |
| **WiFi Mode** | 802.11ac | Value=14 in config |
| **Security** | **WPA-PSK/WPA2-PSK** (mixed) | BeaconType=WPAPSKWPA2PSK |
| **Encryption** | AES | All three WPA cipher fields = AES |
| **Channel** | AUTO (0) | Available: 36-64, 149-165 |
| **Channel Bandwidth** | 20/40/80 MHz | VHT 80MHz supported |
| **Guard Interval** | Short | |
| **Max Stations** | 32 | Current value field shows 41 (may be configured higher) |
| **WPS** | Enabled, PBC mode | |
| **WMM** | Enabled | |
| **Per-SSID MAC Filter** | **Disabled** | WLAN_FltActive=0 |
| **WDS** | Disabled | |
| **PMF/802.11w** | **Not supported** | No PMF/MFP settings exist in router UI |
| **WPA3/SAE** | **Not supported** | No WPA3 options available |
| **Band Steering** | **Not present** | No band steering settings in this router |
| **BSSIDs** | 4 (CMCC-Pg2Y-5G-FAST, CMCC-AP2-5G, CMCC-AP3-5G, CMCC-AP4-5G) | |

## 2.4GHz WiFi Configuration (CMCC-Pg2Y-2.4G)

| Setting | Value |
|---------|-------|
| Enabled | Yes |
| SSID | CMCC-Pg2Y-2.4G |
| Mode | b/g/n (mode=9) |
| Security | WPA-PSK/WPA2-PSK, AES |
| Channel | AUTO |
| TX Power | 100% |
| SSID Hidden | No |

## Global Security Settings

| Setting | Value |
|---------|-------|
| **MAC Filtering** | **Disabled** (enableFilter=0) |
| Filter Mode | White list (no entries) |
| Filter Rules | Empty (MacFilter = null) |

---

## Impact on Auth Frame TX Issue

### Router is NOT causing the auth failure

1. **Security is WPA-PSK/WPA2-PSK (not WPA3/SAE)**
   - Open System Authentication (algorithm=0) is the **correct** auth type for WPA/WPA2
   - The AP should accept algorithm=0 auth frames and respond with auth response
   - SAE (Simultaneous Authentication of Equals) is NOT required

2. **No PMF/802.11w**
   - Router doesn't support 802.11w Protected Management Frames
   - Auth frames do NOT need to be encrypted or protected
   - No IGTK/BIP required

3. **No MAC filtering**
   - Our device MAC address is not being blocked
   - No whitelist/blacklist is active

4. **No band steering**
   - Router is not steering connections between bands
   - Won't reject 5GHz connections

5. **Max clients not reached**
   - Max is 32 stations, unlikely to be full on home network

6. **Channel AUTO**
   - AP auto-selects channel, so our driver must correctly determine
     the AP's operating channel from beacon/probe response before TX
   - This is normal behavior — scan results should have the correct channel

### Conclusion

**The problem is 100% in our driver's TX path, not router-side.**

The router uses standard WPA-PSK/WPA2-PSK with no special security features.
Any 802.11 device sending a standard Open System Authentication frame
(auth algorithm=0, seq=1) on the correct channel should receive an auth response.

The fact that our TXFREE returns stat=1 (30 retries, no ACK) and MIB counters
show zero means the frame is never making it to the RF frontend — this is a
firmware/DMA/TXD formatting issue in our driver, not a compatibility issue.

### What the router config tells us about frame format

For successful authentication with this AP:
- Auth frame: algorithm=0 (Open System), seq_num=1, status=0
- Frame must be sent on the AP's current operating channel
- No special encryption or protection needed for the auth frame
- After auth succeeds: association, then 4-way WPA2 handshake
- AES/CCMP will be used for data encryption after association
