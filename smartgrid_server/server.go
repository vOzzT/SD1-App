package main

import (
	"database/sql"
	"fmt"
	"log"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"

	"github.com/dgrijalva/jwt-go"
	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
	"github.com/joho/godotenv"
	_ "github.com/lib/pq"
	"golang.org/x/crypto/bcrypt"
)

var jwtSecret []byte
var deviceConnections = make(map[string]*websocket.Conn) // Map to store WebSocket connections for devices
var db *sql.DB

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool {
		return true // Allow connections from any origin
	},
}

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
	ID     int    `json:"id"`
	Name   string `json:"name"`
	IPAddr string `json:"ip_addr"`
	UserID int    `json:"user_id"`
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
		IPAddr string `json:"ip_addr" binding:"required"`
		UserID int    `json:"user_id" binding:"required"`
	}

	var input DeviceInput
	if err := c.BindJSON(&input); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid input"})
		return
	}

	sqlStatement := `INSERT INTO devices (name, ip_addr, user_id) VALUES ($1, $2, $3) RETURNING id`
	var deviceID int
	err := db.QueryRow(sqlStatement, input.Name, input.IPAddr, input.UserID).Scan(&deviceID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Could not create device"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Device created successfully", "device_id": deviceID})
}

func readDevice(c *gin.Context) {
	deviceID := c.Param("id")
	var device Device

	sqlStatement := `SELECT id, name, ip_addr, user_id FROM devices WHERE id = $1`
	err := db.QueryRow(sqlStatement, deviceID).Scan(&device.ID, &device.Name, &device.IPAddr, &device.UserID)
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
		Name   string `json:"name"`
		IPAddr string `json:"ip_addr"`
	}

	var deviceUpdate DeviceUpdate
	if err := c.BindJSON(&deviceUpdate); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid input"})
		return
	}

	sqlStatement := `UPDATE devices SET name = $1, ip_addr = $2 WHERE id = $3`
	res, err := db.Exec(sqlStatement, deviceUpdate.Name, deviceUpdate.IPAddr, deviceID)
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

func sendPacket(c *gin.Context) {
	deviceID := c.Param("id")

	// Fetch the IP address of the device from the database using deviceID
	var deviceIP string
	err := db.QueryRow(`SELECT ip_addr FROM devices WHERE id = $1`, deviceID).Scan(&deviceIP)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}

	// Dummy packet data to turn on the LED
	payload := `{"action": "turn_on_led"}`

	// Define the ESP32 endpoint and prepare the HTTP request
	url := fmt.Sprintf("http://%s/led", deviceIP)
	req, err := http.NewRequest("POST", url, strings.NewReader(payload))
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to create request"})
		return
	}
	req.Header.Set("Content-Type", "application/json")

	// Set a timeout for the HTTP client
	client := &http.Client{Timeout: 10 * time.Second}

	// Send the packet to the ESP32
	resp, err := client.Do(req)
	if err != nil {
		log.Println("Error sending packet:", err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to send packet to ESP32"})
		return
	}
	defer resp.Body.Close()

	// Check the ESP32 response status
	if resp.StatusCode == http.StatusOK {
		c.JSON(http.StatusOK, gin.H{"message": "Packet sent successfully", "packet": payload})
	} else {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "ESP32 failed to process packet"})
	}
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
	sqlStatement := `SELECT id, name, ip_addr, user_id FROM devices WHERE user_id = $1`
	rows, err := db.Query(sqlStatement, userID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to fetch devices"})
		return
	}
	defer rows.Close()

	for rows.Next() {
		var device Device
		if err := rows.Scan(&device.ID, &device.Name, &device.IPAddr, &device.UserID); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to parse device data"})
			return
		}
		devices = append(devices, device)
	}

	c.JSON(http.StatusOK, devices)
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
	router.POST("/createUser", createUser)       // C USER
	router.GET("/searchUser", readUser)          // R USER
	router.PUT("/updateUser/:id", updateUser)    // U USER
	router.DELETE("/deleteUser/:id", deleteUser) // D USER

	router.POST("/createDevice", createDevice)       // C DEVICE
	router.GET("/readDevice/:id", readDevice)        // R DEVICE
	router.PUT("/updateDevice/:id", updateDevice)    // U DEVICE
	router.DELETE("/deleteDevice/:id", deleteDevice) // D DEVICE

	// Other routes...
	router.POST("/login", login)

	// Protected route to fetch devices, requires valid JWT
	router.GET("/fetchDevices", AuthMiddleware(), fetchDevices)

	// In your main function, add the endpoint
	router.POST("/sendPacket/:id", sendPacket)

	router.Run(":8080")
}
