# SP01 field DI polarity correction

Field observation: HMI showed DI1..DI8 ON with all passive contacts open.

Known physical input behavior from prior bench evidence is active-low in passive-contact mode: open input reads high, closed/active input reads low. The field prototype had `CONFIG_SP01_DI_INVERT_MASK` at the default `0x00`, so logical status mirrored the raw high level.

Correction: set `CONFIG_SP01_DI_INVERT_MASK=0xFF` in `firmware/esp32-s3/sdkconfig.defaults`.

Expected logical HMI result after flashing corrected image:
- all contacts open -> DI1..DI8 OFF
- one contact active/closed -> only the matching DI becomes ON

Correction commit: `9ba13966337720c2ba459bc057bdee524438797a`
CI run: `34709774512` (`fw #273`) — host and ESP32-S3 jobs PASS.
Artifact: `sp01-field-prototype`, id `10302922425`, sha256 `b79229fe9ccf8e533d9701a87cc1b880b32fe74f52f8842f40ee14f8e5fd405f`.
