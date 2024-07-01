// Copyright(c) 2024 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"context"
	"fmt"
	"flag"
	"os"
	"os/signal"
	"strconv"
	"syscall"
	"time"
)

// Cooling profile struct
type coolingMode struct {
	name   string  // Profile name
	points [][]int // Mapping points (Temp,PWM)
}

// FAN controlling struct
type fanCTRL struct {
	devPWM        string      // PWM device
	devThermal    string      // Thermal device
	cmode         coolingMode // Cooling profile
	checkInterval int         // Pooling interval (in seconds)
}

// Thermal zone path to read temperature
const thermalDevice = string("/sys/devices/virtual/thermal/thermal_zone%d/temp")
// PWM fan device path
const fanSysDir = string("/sys/devices/platform/pwm-fan/hwmon/")
// Maximum PWM value
const maxPWM = 255

// Quiet profile
// Data Source: https://docs.nvidia.com/jetson/archives/r35.4.1/DeveloperGuide/text/SD/PlatformPowerAndPerformance/JetsonXavierNxSeriesAndJetsonAgxXavierSeries.html
func getQuietProfile() coolingMode {
	modeQuiet := coolingMode{
		name: "quiet",
		// Must be sorted!
		points: [][]int{
			{0,0},
			{50,77},
			{63,120},
			{72,160},
			{81,255},
			{140,255},
			{150,255},
			{160,255},
			{170,255},
			{180,255},
		},
	}
	return modeQuiet
}

// Cool profile
// Data Source: https://docs.nvidia.com/jetson/archives/r35.4.1/DeveloperGuide/text/SD/PlatformPowerAndPerformance/JetsonXavierNxSeriesAndJetsonAgxXavierSeries.html
func getCoolProfile() coolingMode {
	modeCool := coolingMode{
		name: "cool",
		// Must be sorted!
		points: [][]int{
			{35,77},
			{53,120},
			{62,160},
			{73,255},
			{140,255},
			{150,255},
			{160,255},
			{170,255},
			{180,255},
		},
	}
	return modeCool
}

// Check if the temperature file exist for a thermal zone
func checkThermalZone(tzone int) bool {
	if _, err := os.Stat(getThermalDevice(tzone)); err == nil {
		return true
	}
	return false
}

// Return the thermal zone temperature file
func getThermalDevice(tzone int) string {
	return fmt.Sprintf(thermalDevice, tzone)
}

// Search for the PWM Fan device
func getFANDevice() (string, error) {
	entries, err := os.ReadDir(fanSysDir)
	if err != nil {
		return "", err
	}
	// We just need the first entry (most probably the only one)
	if len(entries) >= 1 {
		dev := fanSysDir + entries[0].Name() + "/pwm1"
		return dev, nil
	}
	return "", nil
}

// Read and convert (to Celsius) the temperature from a thermal device
func (pwm fanCTRL) readTemp() (int, error) {
	data, err := os.ReadFile(pwm.devThermal)
	if err != nil {
		return -1, err
	}

	str := string(data[:len(data)-1])
	temp, errconv := strconv.Atoi(str)
	if errconv != nil {
		return -1, errconv
	}

	return temp/1000, nil
}

// Set PWM value of a FAN device
func (pwm fanCTRL) setPWM(pwmValue int) error {
	errF := os.WriteFile(pwm.devPWM, []byte(strconv.Itoa(pwmValue)), 0644)
	if errF != nil {
		return fmt.Errorf("Fail to change FAN speed")
	}
	return nil
}

// Perform the controlling iteration
func (pwm fanCTRL) controlPWM() error {
	temp, errT := pwm.readTemp()
	if errT != nil {
		return fmt.Errorf("Fail to read temperature!")
	}

	pwmValue := maxPWM // Set to maximum
	for _, v := range pwm.cmode.points {
		if temp < v[0] {
			pwmValue = v[1]
			break
		}
	}

	return pwm.setPWM(pwmValue)
}

// Controlling loop
func (pwm fanCTRL) run(ctx context.Context) error {
	for {
		select {
		case <-ctx.Done():
			return nil

		case <-time.Tick(time.Duration(pwm.checkInterval) * time.Second):
			err := pwm.controlPWM()
			if err != nil {
				fmt.Println("Error:", err)
			}
		}
	}
}

// Stop FAN controller and set PWM to maximum
func (pwm fanCTRL) finish() error {
	fmt.Println("Finishing FAN controller")
	return pwm.setPWM(maxPWM)
}

func main() {
	var mode string
	var tzone int
	var checkInterval int
	var cmode coolingMode

	// Build and validate the command line
	flag.Usage = func() {
		fmt.Printf("FAN PWM controller for Jetson devices\n\n")
		fmt.Printf("Use:\n    %s [-m <mode>] [-t <thermal_zone>] [-i <pooling_time>]\n\n", os.Args[0])
		flag.PrintDefaults()
	}
	flag.StringVar(&mode, "m", "quiet", "Cooling profile: quiet or cool")
	flag.IntVar(&tzone, "t", 0, "Thermal zone number")
	flag.IntVar(&checkInterval, "i", 2, "Pooling time (in seconds)")
	flag.Parse()

	switch (mode) {
	case "quiet":
		cmode = getQuietProfile()
	case "cool":
		cmode = getCoolProfile()
	default:
		fmt.Fprintf(os.Stderr, "Invalid cooling profile: %s\n", mode)
		os.Exit(1)
	}
	if !checkThermalZone(tzone) {
		fmt.Fprintf(os.Stderr, "Invalid thermal zone: %d\n", tzone)
		os.Exit(1)
	}
	if checkInterval <= 0 || checkInterval >= 60 {
		fmt.Fprintf(os.Stderr, "Invalid pooling time: %d\n", checkInterval)
		os.Exit(1)
	}

	fmt.Println("Starting FAN controller")

	// Search the PWM fan device
	fanDevice, err := getFANDevice()
	if err != nil {
		fmt.Fprintf(os.Stderr, "Could not found a FAN device to control\n")
		os.Exit(1)
	}

	// FAN controller
	pwmCTRL := fanCTRL{
		devPWM: fanDevice,
		devThermal: getThermalDevice(tzone),
		cmode: cmode,
		checkInterval: checkInterval,
	}

	ctx := context.Background()
	ctx, cancel := context.WithCancel(ctx)
	sigChan := make(chan os.Signal, 1)
    signal.Notify(sigChan, os.Interrupt, syscall.SIGHUP)
	defer func(c chan<- os.Signal) {
		signal.Stop(c)
		cancel()
	}(sigChan)

	go func() {
		for {
			select {
			case s := <-sigChan:
				if (s == os.Interrupt) {
					cancel()
					if err := pwmCTRL.finish(); err != nil {
						fmt.Println("Error:", err)
					}
					os.Exit(0)
				}
			case <-ctx.Done():
				if err := pwmCTRL.finish(); err != nil {
					fmt.Println("Error:", err)
				}
			}
		}
	}()

	if err := pwmCTRL.run(ctx); err != nil {
		fmt.Println("Error:", err)
		os.Exit(1)
	}
	if err := pwmCTRL.finish(); err != nil {
		fmt.Println("Error:", err)
		os.Exit(1)
	}
}
