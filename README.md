# Download Complete Dataset
Due to large files being managed by Git LFS, please use the following command to obtain the complete data:
```bash
git clone https://github.com/APM-Z/MRCB.git
```
Alternatively, you can download individual data files directly from the Testdata/ folder in the repository.



# 1 Overview

Multi-system and Multi-frequency Receiver Code Bias Analysis Software (TdRCB) is an open-source GNSS data processing package specifically designed for extracting time-varying receiver code biases (RCBs). TdRCB is developed in C/C++, which can be easily ported to different operating systems, such as Windows and Linux. It is a post-processing software, which can process multi-frequency data from GPS, Galileo, BDS, GLONASS. Using a least-squares filter (LSF) based on undifferenced and uncombined observations to estimate time-varying RCBs. Furthermore, it requires observation files and broadcast ephemeris, offering a simple operation process. Processing strategies for different data can be configured in the formatted configuration file.

# 2 The project directory structure is as follows.
<img width="904" height="630" alt="filestructure" src="https://github.com/user-attachments/assets/698f766d-efd4-48da-a254-beb176a85334" />

