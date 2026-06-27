package nguyenducan;

import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class An9 {

    static List<String[]> results = new ArrayList<>();

    static void check(String given, String when, String actual, String expected) {
        String pass = actual.equals(expected) ? "Pass" : "Fail";
        results.add(new String[]{given, when, actual, expected, pass});
    }

    @Test
    void testFibonacci() {
        Advance3 x = new Advance3();
        int result = x.fibonacci(6);
        check("n=6", "fibonacci(6)", String.valueOf(result), "8");
        assertEquals(8, result);
    }

    @Test
    void testFibonacciNegative() {
        Advance3 x = new Advance3();
        int result = x.fibonacci(-1);
        check("n=-1", "fibonacci(-1)", String.valueOf(result), "-1");
        assertEquals(-1, result);
    }

    @Test
    void testFibonacciZero() {
        Advance3 x = new Advance3();
        int result = x.fibonacci(0);
        check("n=0", "fibonacci(0)", String.valueOf(result), "0");
        assertEquals(0, result);
    }

    @AfterAll
    static void exportReport() throws IOException {
        StringBuilder html = new StringBuilder();
        html.append("<!DOCTYPE html><html><head><meta charset='UTF-8'><style>body{font-family:Arial;padding:30px;}h2{color:#333;}table{border-collapse:collapse;width:100%;}th{background:#4CAF50;color:white;padding:10px;border:1px solid #ccc;}td{border:1px solid #ccc;padding:8px;text-align:center;}tr:nth-child(even){background:#f9f9f9;}.pass{color:green;font-weight:bold;}.fail{color:red;font-weight:bold;}</style></head><body>")
            .append("<h2>Test Report - Question 9: Advance3 (fibonacci)</h2>")
            .append("<table><tr><th>Given</th><th>When</th><th>Actual Result</th><th>Expect Result</th><th>Pass/Fail</th></tr>");
        for (String[] r : results) {
            String css = r[4].equals("Pass") ? "pass" : "fail";
            html.append("<tr><td>").append(r[0]).append("</td><td>").append(r[1]).append("</td><td>").append(r[2]).append("</td><td>").append(r[3]).append("</td><td class='").append(css).append("'>").append(r[4]).append("</td></tr>");
        }
        html.append("</table></body></html>");
        try (FileWriter fw = new FileWriter("test-report-q9.html")) { fw.write(html.toString()); }
        System.out.println(">>> Report saved: test-report-q9.html");
    }
}