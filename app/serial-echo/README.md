# Overview

This application echoes data sent on the serial port `uart0`.

# Requirements

This application has been tested on the following variant of the Zephyr VEE Port:

- Nordic nRF54L15 DK

# Usage

From the root gradle project run the following commands:

```
gradlew :app:serial-echo:runOnSimulator
```

To run the application on simulator or:

```
gradlew :app:serial-echo:runOnDevice
```

To run the application on device.

# Dependencies

_All dependencies are retrieved transitively by Gradle._

# Source

N/A

# Restrictions

None.
 
---  

_Markdown_  
_Copyright 2026 MicroEJ Corp. All rights reserved._  
_Use of this source code is governed by a BSD-style license that can be found with this software._  
