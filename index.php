<?php
// Set headers to always return a JSON response
header('Content-Type: application/json; charset=utf-8');

// 1. Enforce that only POST requests are allowed
if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') {
    http_response_code(405); // Method Not Allowed
    echo json_encode([
        'status' => 'error',
        'message' => 'Only POST requests are allowed.'
    ]);
    exit();
}

// Initialize an array to capture the parsed data
$receivedData = [];
$contentType = $_SERVER['CONTENT_TYPE'] ?? '';

// 2. Handle JSON payload (e.g., from API clients, mobile apps, or JavaScript fetch)
if (str_contains($contentType, 'application/json')) {
    $rawBody = file_get_contents('php://input'); // Read raw body stream
    
    try {
        $receivedData = json_decode($rawBody, true, 512, JSON_THROW_ON_ERROR);
    } catch (JsonException $e) {
        http_response_code(400); // Bad Request
        echo json_encode([
            'status' => 'error',
            'message' => 'Malformed JSON payload: ' . $e->getMessage()
        ]);
        exit();
    }
} 
// 3. Handle standard HTML Form / URL-encoded data
else {
    // $_POST autoglobal handles application/x-www-form-urlencoded and multipart/form-data
    $receivedData = $_POST;
}
// print_r(json_encode($receivedData));

// 4. Validate or process the data (Example: check if a 'username' was passed)
if (empty($receivedData)) {
    http_response_code(400);
    echo json_encode([
        'status' => 'error',
        'message' => 'No data received.'
    ]);
    exit();
}

// 5. Sanitize data before outputting to prevent XSS (if reflecting data back)
$sanitizedData = array_map(function($value) {
    return is_string($value) ? htmlspecialchars($value, ENT_QUOTES, 'UTF-8') : $value;
}, $receivedData);
// print_r(json_encode($sanitizedData));
error_log(print_r($sanitizedData, true));


// 6. Return successful response
http_response_code(200); // OK
echo json_encode([
    'status' => 'success',
    // 'message' => 'POST request received successfully!',
    // 'data_received' => $sanitizedData
], JSON_PRETTY_PRINT);
