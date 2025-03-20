package main

import (
	"database/sql"
	"fmt"
	"log"
	"net/http"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/dgrijalva/jwt-go"
	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
	"github.com/joho/godotenv"
	_ "github.com/lib/pq"
	"golang.org/x/crypto/bcrypt"
)

var db *sql.DB
var jwtSecret []byte

// WebSocket upgrader
var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool { return true },
}

var (
	deviceConnections = make(map[string]*websocket.Conn) // Maps MAC to WebSocket
	mu                sync.Mutex
)

// JWT Claims struct
type Claims struct {
	Login  string `json:"login"`
	UserID int    `json:"user_id"`
	jwt.StandardClaims
}

// Login struct to bind JSON
type Login struct {
	Login string `json:"login"`
	Pass  string `json:"pass"`
}

type User struct {
	ID         int    `json:"id"`
	Name       string `json:"name"`
	Email      string `json:"email"`
	Login      string `json:"login"`
	Pass       string `json:"pass"`
	IsVerified bool   `json:"isverified"`
}

type Device struct {
	ID      int    `json:"id"`
	Name    string `json:"name"`
	MACAddr string `json:"mac_addr"`
	UserID  int    `json:"user_id"`
}

type Breaker struct {
	ID             int    `json:"id"`
	DeviceID       int    `json:"device_id"`
	Name           string `json:"name"`
	Breaker_Number string `json:"breaker_number"`
	Status         bool   `json:"status"`
}

// Load environment variables from .env file
func init() {
	// Load environment variables from .env file
	err := godotenv.Load(".env")
	if err != nil {
		log.Fatalf("Error loading .env file: %v", err)
	}

	// Set JWT secret
	jwtSecret = []byte(os.Getenv("JWT_SECRET"))
}

func handleWebSocket(w http.ResponseWriter, r *http.Request) {
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Println("WebSocket upgrade failed:", err)
		return
	}

	// Read MAC address from device
	_, msg, err := conn.ReadMessage()
	if err != nil {
		log.Println("Failed to read MAC address:", err)
		conn.Close()
		return
	}
	macAddress := string(msg)

	// Check if this MAC is already linked to a device
	var deviceID, userID int
	err = db.QueryRow(`SELECT id, user_id FROM devices WHERE mac_addr = $1`, macAddress).Scan(&deviceID, &userID)
	if err == sql.ErrNoRows {
		// MAC is new, find the first device without a MAC and link it
		err = db.QueryRow(`UPDATE devices SET mac_addr = $1 WHERE mac_addr IS NULL RETURNING id, user_id`, macAddress).Scan(&deviceID, &userID)
		if err != nil {
			log.Println("No available device entry to update for MAC:", macAddress)
			conn.Close()
			return
		}
		log.Printf("Linked MAC %s to device %d\n", macAddress, deviceID)
	} else if err != nil {
		log.Println("Database error:", err)
		conn.Close()
		return
	}

	// Close existing connection if device is reconnecting
	mu.Lock()
	if oldConn, exists := deviceConnections[macAddress]; exists {
		oldConn.Close() // Properly close the old connection
	}
	deviceConnections[macAddress] = conn
	mu.Unlock()

	log.Println("Device connected:", macAddress)

	// Keep connection alive
	for {
		_, _, err := conn.ReadMessage()
		if err != nil {
			log.Println("Device disconnected:", macAddress)
			mu.Lock()
			delete(deviceConnections, macAddress)
			mu.Unlock()
			conn.Close()
			break
		}
	}
}

func createUser(c *gin.Context) {
	var user User
	if err := c.BindJSON(&user); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid input"})
		return
	}

	// Hash the password before storing it
	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(user.Pass), bcrypt.DefaultCost)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Could not hash password"})
		return
	}
	user.Pass = string(hashedPassword)

	// Prepare the SQL statement for inserting the user
	sqlStatement := `INSERT INTO users (name, email, login, pass, isverified) VALUES ($1, $2, $3, $4, $5) RETURNING id`
	err = db.QueryRow(sqlStatement, user.Name, user.Email, user.Login, user.Pass, user.IsVerified).Scan(&user.ID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Could not create user"})
		return
	}

	// Respond with success
	c.JSON(http.StatusOK, gin.H{"message": "User created successfully", "user_id": user.ID})
}

func readUser(c *gin.Context) { // Search User
	searchParam := c.Query("query")

	sqlStatement := `
        SELECT id, name, email, login, pass, isverified
        FROM users
        WHERE name ILIKE $1 OR email ILIKE $1 OR login ILIKE $1`

	rows, err := db.Query(sqlStatement, "%"+searchParam+"%")
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to search users"})
		return
	}
	defer rows.Close()

	var users []User
	for rows.Next() {
		var user User
		if err := rows.Scan(&user.ID, &user.Name, &user.Email, &user.Login, &user.Pass, &user.IsVerified); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to scan user"})
			return
		}
		users = append(users, user)
	}

	c.JSON(http.StatusOK, users)
}

func updateUser(c *gin.Context) {
	idParam := c.Param("id")
	id, err := strconv.Atoi(idParam)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid user ID"})
		return
	}

	var user User
	if err := c.BindJSON(&user); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid input"})
		return
	}

	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(user.Pass), bcrypt.DefaultCost)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Could not hash password"})
		return
	}
	user.Pass = string(hashedPassword)

	sqlStatement := `
        UPDATE users
        SET name = $1, email = $2, login = $3, pass = $4, isverified = $5
        WHERE id = $6`
	res, err := db.Exec(sqlStatement, user.Name, user.Email, user.Login, user.Pass, user.IsVerified, id)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update user"})
		return
	}

	rowsAffected, err := res.RowsAffected()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to retrieve affected rows"})
		return
	}

	if rowsAffected == 0 {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "User updated successfully"})
}

func deleteUser(c *gin.Context) {
	idParam := c.Param("id")
	id, err := strconv.Atoi(idParam)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid user ID"})
		return
	}

	sqlStatement := `DELETE FROM users WHERE id = $1`
	res, err := db.Exec(sqlStatement, id)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to delete user"})
		return
	}

	rowsAffected, err := res.RowsAffected()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to retrieve affected rows"})
		return
	}

	if rowsAffected == 0 {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "User deleted successfully"})
}

func login(c *gin.Context) {
	var login Login
	if err := c.BindJSON(&login); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid input"})
		return
	}

	var user User
	sqlStatement := `SELECT id, name, email, login, pass, isverified FROM users WHERE login = $1`
	row := db.QueryRow(sqlStatement, login.Login)
	if err := row.Scan(&user.ID, &user.Name, &user.Email, &user.Login, &user.Pass, &user.IsVerified); err != nil {
		if err == sql.ErrNoRows {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Incorrect login or password"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Database error"})
		return
	}

	if err := bcrypt.CompareHashAndPassword([]byte(user.Pass), []byte(login.Pass)); err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Incorrect login or password"})
		return
	}

	expirationTime := time.Now().Add(24 * time.Hour)
	claims := &Claims{
		UserID: user.ID,
		StandardClaims: jwt.StandardClaims{
			ExpiresAt: expirationTime.Unix(),
		},
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	tokenString, err := token.SignedString(jwtSecret)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Could not create token"})
		return
	}

	// Update the JWT token in the database
	updateTokenSQL := `UPDATE users SET jwt = $1 WHERE id = $2`
	_, err = db.Exec(updateTokenSQL, &tokenString, &user.ID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Could not update JWT token"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"token": tokenString, "userID": claims.UserID})
}

func createDevice(c *gin.Context) {
	type DeviceInput struct {
		Name   string `json:"name" binding:"required"`
		UserID int    `json:"user_id" binding:"required"`
	}

	var input DeviceInput
	if err := c.ShouldBindJSON(&input); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid input"})
		return
	}

	sqlStatement := `INSERT INTO devices (name, user_id) VALUES ($1, $2) RETURNING id`
	fmt.Println(sqlStatement, input.Name, input.UserID)
	var deviceID int
	err := db.QueryRow(sqlStatement, input.Name, input.UserID).Scan(&deviceID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Could not create device"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Device created successfully", "device_id": deviceID})
}

func readDevice(c *gin.Context) {
	deviceID := c.Param("id")
	var device Device

	sqlStatement := `SELECT id, name, mac_addr, user_id FROM devices WHERE id = $1`
	err := db.QueryRow(sqlStatement, deviceID).Scan(&device.ID, &device.Name, &device.MACAddr, &device.UserID)
	if err != nil {
		if err == sql.ErrNoRows {
			c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Could not retrieve device"})
		return
	}

	c.JSON(http.StatusOK, device)
}

func updateDevice(c *gin.Context) {
	deviceID := c.Param("id")

	type DeviceUpdate struct {
		Name string `json:"name"`
	}

	var deviceUpdate DeviceUpdate
	if err := c.BindJSON(&deviceUpdate); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid input"})
		return
	}

	sqlStatement := `UPDATE devices SET name = $1 WHERE id = $2`
	res, err := db.Exec(sqlStatement, deviceUpdate.Name, deviceID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Could not update device"})
		return
	}

	rowsAffected, _ := res.RowsAffected()
	if rowsAffected == 0 {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Device updated successfully"})
}

func deleteDevice(c *gin.Context) {
	// Get the device ID from the URL parameter
	deviceID := c.Param("id")

	// Delete the device from the database
	sqlStatement := `DELETE FROM devices WHERE id = $1`
	res, err := db.Exec(sqlStatement, deviceID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Could not delete device"})
		return
	}

	rowsAffected, _ := res.RowsAffected()
	if rowsAffected == 0 {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Device deleted successfully"})
}

func createBreaker(c *gin.Context) {
	type BreakerInput struct {
		DeviceID   int    `json:"device_id" binding:"required"`
		Name       string `json:"name" binding:"required"`
		BreakerNum string `json:"breaker_number" binding:"required"`
	}

	var input BreakerInput
	if err := c.ShouldBindJSON(&input); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid input"})
		return
	}

	sqlStatement := `INSERT INTO breakers (name, device_id, breaker_number) VALUES ($1, $2, $3) RETURNING id`
	fmt.Println(sqlStatement, input.Name, input.DeviceID, input.BreakerNum)
	var breakerID int
	err := db.QueryRow(sqlStatement, input.Name, input.DeviceID, input.BreakerNum).Scan(&breakerID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Could not create device"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Breaker created successfully", "breakerID": breakerID})
}

func readBreaker(c *gin.Context) {
	breakerID := c.Param("id")
	var breaker Breaker

	sqlStatement := `SELECT id, name, device_id, breaker_number FROM breakers WHERE id = $1`
	err := db.QueryRow(sqlStatement, breakerID).Scan(&breaker.ID, &breaker.Name, &breaker.DeviceID, &breaker.Breaker_Number)
	if err != nil {
		if err == sql.ErrNoRows {
			c.JSON(http.StatusNotFound, gin.H{"error": "Breaker not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Could not retrieve device"})
		return
	}

	c.JSON(http.StatusOK, breaker)
}

func updateBreaker(c *gin.Context) {
	breakerID := c.Param("id")

	type BreakerUpdate struct {
		Name          string `json:"name"`
		BreakerNumber string `json:"breaker_number"`
	}

	var breakerUpdate BreakerUpdate
	if err := c.BindJSON(&breakerUpdate); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid input"})
		return
	}

	sqlStatement := `UPDATE breakers SET name = $1, breaker_number = $2 WHERE id = $3`
	res, err := db.Exec(sqlStatement, breakerUpdate.Name, breakerUpdate.BreakerNumber, breakerID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Could not update breaker"})
		return
	}

	rowsAffected, _ := res.RowsAffected()
	if rowsAffected == 0 {
		c.JSON(http.StatusNotFound, gin.H{"error": "Breaker not found"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Breaker updated successfully"})
}

func deleteBreaker(c *gin.Context) {
	// Get the device ID from the URL parameter
	breakerID := c.Param("id")

	// Delete the device from the database
	sqlStatement := `DELETE FROM breakers WHERE id = $1`
	res, err := db.Exec(sqlStatement, breakerID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Could not delete device"})
		return
	}

	rowsAffected, _ := res.RowsAffected()
	if rowsAffected == 0 {
		c.JSON(http.StatusNotFound, gin.H{"error": "Breaker not found"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Breaker deleted successfully"})
}

func sendPacket(c *gin.Context) {
	deviceID := c.Param("id")

	// Get MAC address from database
	var macAddr string
	err := db.QueryRow(`SELECT mac_addr FROM devices WHERE id = $1`, deviceID).Scan(&macAddr)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}

	// Check if the device is connected via WebSocket
	mu.Lock()
	conn, exists := deviceConnections[macAddr]
	mu.Unlock()

	if !exists {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not connected"})
		return
	}

	// Parse user command
	var reqBody struct {
		Command   string `json:"command"`
		BreakerID *int   `json:"breakerId,omitempty"`
	}
	if err := c.ShouldBindJSON(&reqBody); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid request format"})
		return
	}

	// Create payload
	var payload string
	switch reqBody.Command {
	case "pingDevice":
		payload = `{"command": "pingDevice"}`
	case "flashLED":
		payload = `{"command": "flashLED"}`
	case "toggleBreaker":
		if reqBody.BreakerID == nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": "Breaker ID required"})
			return
		}
		payload = fmt.Sprintf(`{"command": "toggleBreaker", "breakerId": %d}`, *reqBody.BreakerID)
	default:
		c.JSON(http.StatusBadRequest, gin.H{"error": "Unknown command"})
		return
	}

	// Send command via WebSocket
	err = conn.WriteMessage(websocket.TextMessage, []byte(payload))
	if err != nil {
		log.Println("WebSocket write failed for:", macAddr, err)
		mu.Lock()
		delete(deviceConnections, macAddr) // Remove stale connection
		mu.Unlock()
		conn.Close()
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Device disconnected"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Command sent successfully"})

}

func AuthMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" || !strings.HasPrefix(authHeader, "Bearer ") {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Authorization header missing or invalid"})
			c.Abort()
			return
		}

		tokenString := strings.TrimPrefix(authHeader, "Bearer ")
		claims := &Claims{}
		token, err := jwt.ParseWithClaims(tokenString, claims, func(token *jwt.Token) (interface{}, error) {
			return jwtSecret, nil
		})

		if err != nil || !token.Valid {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid token"})
			c.Abort()
			return
		}

		// Store the user ID in the context
		c.Set("userID", claims.UserID)
		c.Next()
	}
}

func fetchDevices(c *gin.Context) {
	// Retrieve the user ID from the context
	userID, exists := c.Get("userID")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	// Query devices associated with the user
	var devices []Device
	sqlStatement := `SELECT id, user_id, name, mac_addr FROM devices WHERE user_id = $1`
	rows, err := db.Query(sqlStatement, userID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to fetch devices"})
		return
	}
	defer rows.Close()

	for rows.Next() {
		var device Device
		var macAddr sql.NullString // Handles NULL values

		if err := rows.Scan(&device.ID, &device.UserID, &device.Name, &macAddr); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to parse device data"})
			return
		}

		// Convert NullString to standard string (empty string if NULL)
		device.MACAddr = macAddr.String

		devices = append(devices, device)
	}

	c.JSON(http.StatusOK, devices)
}

func fetchBreakers(c *gin.Context) {
	// Retrieve the user ID from the context
	userID, exists := c.Get("userID")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	// Retrieve the device ID from the URL parameter
	deviceID := c.Param("id")
	if deviceID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Device ID is required"})
		return
	}

	// Verify the device belongs to the authenticated user
	var device Device
	sqlStatement := `SELECT id, user_id FROM devices WHERE id = $1 AND user_id = $2`
	err := db.QueryRow(sqlStatement, deviceID, userID).Scan(&device.ID, &device.UserID)
	if err != nil {
		if err == sql.ErrNoRows {
			c.JSON(http.StatusNotFound, gin.H{"error": "Device not found or does not belong to the user"})
		} else {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to verify device"})
		}
		return
	}

	// Query breakers associated with the device ID
	var breakers []Breaker
	sqlStatement = `SELECT id, device_id, name, breaker_number, status FROM breakers WHERE device_id = $1`
	rows, err := db.Query(sqlStatement, deviceID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to fetch breakers"})
		return
	}
	defer rows.Close()

	for rows.Next() {
		var breaker Breaker

		if err := rows.Scan(&breaker.ID, &breaker.DeviceID, &breaker.Name, &breaker.Breaker_Number, &breaker.Status); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to parse breaker data"})
			return
		}

		breakers = append(breakers, breaker)
	}

	// Return the list of breakers as a JSON response
	c.JSON(http.StatusOK, breakers)
}

func connectDB() (*sql.DB, error) {
	connStr := fmt.Sprintf(
		"user=%s password=%s dbname=%s sslmode=%s",
		os.Getenv("DB_USER"),
		os.Getenv("DB_PASSWORD"),
		os.Getenv("DB_NAME"),
		os.Getenv("DB_SSLMODE"),
	)
	db, err := sql.Open("postgres", connStr)
	if err != nil {
		return nil, err
	}

	// Verify connection
	if err := db.Ping(); err != nil {
		return nil, err
	}

	fmt.Println("Connected to PostgreSQL database!")
	return db, nil
}
func main() {
	var err error
	db, err = connectDB()
	if err != nil {
		log.Fatal("Failed to connect to database:", err)
	}
	defer db.Close()

	router := gin.Default()

	// WebSocket endpoint
	router.GET("/ws", func(c *gin.Context) {
		handleWebSocket(c.Writer, c.Request)
	})

	router.POST("/createUser", createUser)       // C USER
	router.GET("/searchUser", readUser)          // R USER
	router.PUT("/updateUser/:id", updateUser)    // U USER
	router.DELETE("/deleteUser/:id", deleteUser) // D USER

	router.POST("/createDevice", createDevice)       // C DEVICE
	router.GET("/readDevice/:id", readDevice)        // R DEVICE
	router.PUT("/updateDevice/:id", updateDevice)    // U DEVICE
	router.DELETE("/deleteDevice/:id", deleteDevice) // D DEVICE

	router.POST("/createBreaker", createBreaker)       // C BREAKER
	router.GET("/readBreaker/:id", readBreaker)        // R DEVICE
	router.PUT("/updateBreaker/:id", updateBreaker)    // U BREAKER
	router.DELETE("/deleteBreaker/:id", deleteBreaker) // D BREAKER

	// Other routes...
	router.POST("/login", login)

	// Protected route to fetch devices, requires valid JWT
	router.GET("/fetchDevices", AuthMiddleware(), fetchDevices)
	router.GET("/fetchBreakers/:id", AuthMiddleware(), fetchBreakers)

	// In your main function, add the endpoint
	router.POST("/sendPacket/:id", sendPacket)

	log.Println("Server is running on :8080")
	if err := router.Run(":8080"); err != nil {
		log.Fatal("Failed to start server:", err)
	}
}
