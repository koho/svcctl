package services

import (
	"errors"
	"golang.org/x/sys/windows"
	"golang.org/x/sys/windows/svc"
	"golang.org/x/sys/windows/svc/mgr"
	"os"
	"svcctl/svcutils"
	"time"
)

func InstallService(address string, token string) error {
	m, err := svcutils.ServiceManager()
	if err != nil {
		return err
	}
	path, err := os.Executable()
	if err != nil {
		return err
	}

	service, err := m.OpenService(ServiceName)
	if err == nil {
		status, err := service.Query()
		if err != nil && err != windows.ERROR_SERVICE_MARKED_FOR_DELETE {
			service.Close()
			return err
		}
		if status.State != svc.Stopped && err != windows.ERROR_SERVICE_MARKED_FOR_DELETE {
			service.Close()
			return errors.New("service already installed and running")
		}
		err = service.Delete()
		service.Close()
		if err != nil && err != windows.ERROR_SERVICE_MARKED_FOR_DELETE {
			return err
		}
		for {
			service, err = m.OpenService(ServiceName)
			if err != nil && err != windows.ERROR_SERVICE_MARKED_FOR_DELETE {
				break
			}
			service.Close()
			time.Sleep(time.Second / 3)
		}
	}

	conf := mgr.Config{
		ServiceType:  windows.SERVICE_WIN32_OWN_PROCESS,
		StartType:    mgr.StartAutomatic,
		ErrorControl: mgr.ErrorNormal,
		DisplayName:  "Service Controller",
		Description:  "Provide a web interface to query/control service.",
		SidType:      windows.SERVICE_SID_TYPE_UNRESTRICTED,
	}
	service, err = m.CreateService(ServiceName, path, conf, "-address", address, "-token", token, "-service")
	if err != nil {
		return err
	}

	service.Close()
	return nil
}

func UninstallService() error {
	m, err := svcutils.ServiceManager()
	if err != nil {
		return err
	}
	service, err := m.OpenService(ServiceName)
	if err != nil {
		return err
	}
	service.Control(svc.Stop)
	err = service.Delete()
	err2 := service.Close()
	if err != nil && err != windows.ERROR_SERVICE_MARKED_FOR_DELETE {
		return err
	}
	return err2
}
