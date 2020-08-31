package api

import (
	"fmt"
	"github.com/gin-gonic/gin"
	"net/http"
	"os"
	"path/filepath"
)

func readFile(c *gin.Context) {
	filePath := c.Query("path")
	_, err := os.Stat(filePath)
	if err != nil {
		c.JSON(http.StatusOK, gin.H{"code": 1, "msg": err.Error()})
		return
	}
	c.Writer.Header().Add("Content-Disposition", fmt.Sprintf("attachment; filename=%s", filepath.Base(filePath)))
	c.Writer.Header().Add("Content-Type", "application/octet-stream")
	c.File(filePath)
}

func writeFile(c *gin.Context) {
	filePath := c.Query("path")
	file, err := c.FormFile("file")
	if err != nil {
		c.JSON(http.StatusOK, gin.H{"code": 1, "msg": err.Error()})
		return
	}
	err = c.SaveUploadedFile(file, filePath)
	if err != nil {
		c.JSON(http.StatusOK, gin.H{"code": 1, "msg": err.Error()})
		return
	}
	c.JSON(http.StatusOK, gin.H{"code": 0, "msg": "OK"})
}
