package api

import (
	"github.com/gin-gonic/gin"
	"net/http"
	"strconv"
	"svcctl/svcutils"
)

var actionMap = map[string]svcutils.Cmd{
	"start": svcutils.Start, "stop": svcutils.Stop,
}

func respondWithError(code int, message string, c *gin.Context) {
	resp := gin.H{"code": code, "msg": message}
	c.JSON(code, resp)
	c.Abort()
}

func TokenAuthMiddleware(token string) gin.HandlerFunc {
	return func(c *gin.Context) {
		authToken := c.Request.Header.Get("Authorization")
		if authToken == "" {
			respondWithError(http.StatusUnauthorized, "API token required", c)
			return
		}
		if authToken != token {
			respondWithError(http.StatusUnauthorized, "Invalid API token", c)
			return
		}
		c.Next()
	}
}

func query(c *gin.Context) {
	name := c.Param("name")
	ret := gin.H{
		"code":   0,
		"msg":    "OK",
		"name":   name,
		"status": false,
	}
	running, err := svcutils.QueryService(name)
	if err != nil {
		ret["code"] = 1
		ret["msg"] = err.Error()
	}
	ret["status"] = running
	c.JSON(http.StatusOK, ret)
}

func control(c *gin.Context) {
	ret := gin.H{
		"code":   0,
		"msg":    "OK",
		"name":   c.Param("name"),
		"action": c.Param("action"),
	}
	timeout := c.Request.FormValue("timeout")
	timeoutSecond, err := strconv.Atoi(timeout)
	if err != nil {
		timeoutSecond = 10
	}
	_, err = svcutils.ControlService(ret["name"].(string), actionMap[ret["action"].(string)], timeoutSecond)
	if err != nil {
		ret["code"] = 1
		ret["msg"] = err.Error()
	}
	c.JSON(http.StatusOK, ret)
}

func ServeAPI(address string, token string) {
	r := gin.Default()
	r.Use(TokenAuthMiddleware(token))
	r.GET("/query/:name", query)
	r.GET("/ctl/:name/:action", control)
	r.Run(address)
}
