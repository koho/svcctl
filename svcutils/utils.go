package svcutils

import (
	"errors"
	"golang.org/x/sys/windows"
	"golang.org/x/sys/windows/svc"
	"golang.org/x/sys/windows/svc/mgr"
	"time"
)

var cachedServiceManager *mgr.Mgr

func ServiceManager() (*mgr.Mgr, error) {
	if cachedServiceManager != nil {
		return cachedServiceManager, nil
	}
	m, err := mgr.Connect()
	if err != nil {
		return nil, err
	}
	cachedServiceManager = m
	return cachedServiceManager, nil
}

type Cmd int32

const (
	Start Cmd = 1
	Stop  Cmd = 2
)

func QueryService(name string) (bool, error) {
	if name == "" {
		return false, errors.New("empty service name")
	}
	m, err := ServiceManager()
	if err != nil {
		return false, err
	}

	service, err := m.OpenService(name)
	if err == nil {
		defer service.Close()
		status, err := service.Query()
		if err != nil && err != windows.ERROR_SERVICE_MARKED_FOR_DELETE {
			return false, err
		}
		return status.State != svc.Stopped && err != windows.ERROR_SERVICE_MARKED_FOR_DELETE, nil
	}
	return false, err
}

func ControlService(name string, action Cmd, timeout int) (bool, error) {
	if name == "" {
		return false, errors.New("empty service name")
	}
	m, err := ServiceManager()
	if err != nil {
		return false, err
	}
	service, err := m.OpenService(name)
	if err == nil {
		defer service.Close()
		switch action {
		case Start:
			err = service.Start()
		case Stop:
			_, err = service.Control(svc.Stop)
		default:
			return false, errors.New("invalid action name")
		}
		if err != nil {
			return false, err
		}
		for i := 0; i < timeout; i++ {
			status, err := service.Query()
			if err != nil {
				return false, err
			}
			if action == Start && status.State == svc.Running {
				return true, nil
			} else if action == Stop && status.State == svc.Stopped {
				return true, nil
			}
			time.Sleep(1 * time.Second)
		}
		return false, errors.New("command timed out")
	}
	return false, err
}
