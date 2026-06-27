package nguyenducan;

import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class An16 {

    static List<String[]> results = new ArrayList<>();

    static void check(String given, String when, String actual, String expected) {
        String pass = actual.equals(expected) ? "Pass" : "Fail";
        results.add(new String[]{given, when, actual, expected, pass});
    }

    @Test
    void testLoginSuccess() {
        boolean result = LoginService.login("user", "password");
        check("username=user, password=password", "login()", String.valueOf(result), "true");
        assertTrue(result);
    }

    @Test
    void testInvalidUsername() {
        boolean result = LoginService.login("invalidUser", "password");
        check("username=invalidUser, password=password", "login()", String.valueOf(result), "false");
        assertFalse(result);
    }

    @Test
    void testInvalidPassword() {
        boolean result = LoginService.login("user", "wrongPassword");
        check("username=user, password=wrongPassword", "login()", String.valueOf(result), "false");
        assertFalse(result);
    }

    @Test
    void testBothInvalid() {
        boolean result = LoginService.login("guest", "123456");
        check("username=guest, password=123456", "login()", String.valueOf(result), "false");
        assertFalse(result);
    }

    @Test
    void testBothEmpty() {
        boolean result = LoginService.login("", "");
        check("username='', password=''", "login()", String.valueOf(result), "false");
        assertFalse(result);
    }

    @Test
    void testEmptyUsername() {
        boolean result = LoginService.login("", "password");
        check("username='', password=password", "login()", String.valueOf(result), "false");
        assertFalse(result);
    }

    @Test
    void testEmptyPassword() {
        boolean result = LoginService.login("user", "");
        check("username=user, password=''", "login()", String.valueOf(result), "false");
        assertFalse(result);
    }

    @Test
    void testUsernameWithSpace() {
        boolean result = LoginService.login(" user ", "password");
        check("username=' user ', password=password", "login()", String.valueOf(result), "false");
        assertFalse(result);
    }

    @AfterAll
    static void exportReport() throws IOException {
        StringBuilder html = new StringBuilder();
        html.append("<!DOCTYPE html><html><head><meta charset='UTF-8'><style>body{font-family:Arial;padding:30px;}h2{color:#333;}table{border-collapse:collapse;width:100%;}th{background:#4CAF50;color:white;padding:10px;border:1px solid #ccc;}td{border:1px solid #ccc;padding:8px;text-align:center;}tr:nth-child(even){background:#f9f9f9;}.pass{color:green;font-weight:bold;}.fail{color:red;font-weight:bold;}</style></head><body>")
            .append("<h2>Test Report - Question 16: LoginService</h2>")
            .append("<table><tr><th>Given</th><th>When</th><th>Actual Result</th><th>Expect Result</th><th>Pass/Fail</th></tr>");
        for (String[] r : results) {
            String css = r[4].equals("Pass") ? "pass" : "fail";
            html.append("<tr><td>").append(r[0]).append("</td><td>").append(r[1]).append("</td><td>").append(r[2]).append("</td><td>").append(r[3]).append("</td><td class='").append(css).append("'>").append(r[4]).append("</td></tr>");
        }
        html.append("</table></body></html>");
        try (FileWriter fw = new FileWriter("test-report-q16.html")) { fw.write(html.toString()); }
        System.out.println(">>> Report saved: test-report-q16.html");
    }
}