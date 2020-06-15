package main

import (
	"flag"
	"github.com/google/uuid"
	"log"
	"svcctl/api"
	"svcctl/services"
)

const (
	defaultAddress = ":9001"
	ver            = "1.0.1"
)

var (
	address   = flag.String("address", defaultAddress, "Address to be serve on.")
	service   = flag.Bool("service", false, "Run as service.")
	token     = flag.String("token", "", "API token for authorization.")
	install   = flag.Bool("install", false, "Install as service.")
	uninstall = flag.Bool("uninstall", false, "Uninstall service.")
	version   = flag.Bool("version", false, "Print version info.")
)

func main() {
	flag.Parse()
	if *version {
		println(ver)
		return
	}
	if *uninstall {
		log.Printf("Uninstalling service %s\n", services.ServiceName)
		if err := services.UninstallService(); err != nil {
			log.Fatal(err)
		}
		log.Println("Done")
		return
	}
	apiAddress := *address
	if apiAddress == "" {
		apiAddress = defaultAddress
	}
	apiToken := *token
	if apiToken == "" {
		apiToken = uuid.New().String()
	}
	log.Printf("Use API token: %s\n", apiToken)
	if *install {
		log.Printf("Installing service %s\n", services.ServiceName)
		if err := services.InstallService(apiAddress, apiToken); err != nil {
			log.Fatal(err)
		}
		log.Println("Done")
		return
	}
	if *service {
		log.Println("Running in service mode")
		if err := services.Run(apiAddress, apiToken); err != nil {
			log.Fatal(err)
		}
	} else {
		log.Println("Running in command-line mode")
		api.ServeAPI(apiAddress, apiToken)
	}
}
