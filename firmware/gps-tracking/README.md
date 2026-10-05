# Planned phone geolocation and caregiver alerts

BLE NUS telemetry is implemented in `../core-sensing/core-sensing.ino`. This directory contains a plan, not a working geolocation app.

Proposed flow: cane event, companion phone receives notification, phone attaches location with accuracy and timestamp, network service delivers an acknowledged caregiver alert. Remote delivery depends on phone availability, permissions and connectivity. Phone location can be unavailable or inaccurate indoors; do not promise an immediate fix.

The design avoids adding a dedicated GNSS/cellular module to the cane, assuming the user has a compatible phone. Validate current consumption, background behavior, reconnects, stale location and failed delivery. Location sharing should be opt-in. Local obstacle feedback remains on the cane.
