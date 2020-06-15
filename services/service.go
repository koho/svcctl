package services

import (
	"golang.org/x/sys/windows/svc"
	"log"
	"svcctl/api"
)

const ServiceName = "svcctl"

type ctlService struct {
	address string
	token   string
}

func (service *ctlService) Execute(args []string, r <-chan svc.ChangeRequest, changes chan<- svc.Status) (svcSpecificEC bool, exitCode uint32) {
	changes <- svc.Status{State: svc.StartPending}

	defer func() {
		changes <- svc.Status{State: svc.StopPending}
		log.Println("Shutting down")
	}()

	go api.ServeAPI(service.address, service.token)

	changes <- svc.Status{State: svc.Running, Accepts: svc.AcceptStop | svc.AcceptShutdown}
	log.Println("Startup complete")

	for {
		select {
		case c := <-r:
			switch c.Cmd {
			case svc.Stop, svc.Shutdown:
				return
			case svc.Interrogate:
				changes <- c.CurrentStatus
			default:
				log.Printf("Unexpected services control request #%d\n", c)
			}
		}
	}
}

func Run(address string, token string) error {
	return svc.Run(ServiceName, &ctlService{address, token})
}
