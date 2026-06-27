package nguyenducan;

import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class An14 {

    static List<String[]> results = new ArrayList<>();

    static void check(String given, String when, String actual, String expected) {
        String pass = actual.equals(expected) ? "Pass" : "Fail";
        results.add(new String[]{given, when, actual, expected, pass});
    }

    @Test
    void testPositiveNumbers() {
        int[] sum1 = {1, 2, 3, 4, 5};
        int result = ArraySum.calculateSum(sum1);
        check("{1,2,3,4,5}", "calculateSum()", String.valueOf(result), "15");
        assertEquals(15, result);
    }

    @Test
    void testMixedNumbers() {
        int[] sum2 = {-1, 0, 1};
        int result = ArraySum.calculateSum(sum2);
        check("{-1,0,1}", "calculateSum()", String.valueOf(result), "0");
        assertEquals(0, result);
    }

    @Test
    void testLargeNumbers() {
        int[] sum3 = {10, 20, 30, 40, 50};
        int result = ArraySum.calculateSum(sum3);
        check("{10,20,30,40,50}", "calculateSum()", String.valueOf(result), "150");
        assertEquals(150, result);
    }

    @AfterAll
    static void exportReport() throws IOException {
        StringBuilder html = new StringBuilder();
        html.append("<!DOCTYPE html><html><head><meta charset='UTF-8'><style>body{font-family:Arial;padding:30px;}h2{color:#333;}table{border-collapse:collapse;width:100%;}th{background:#4CAF50;color:white;padding:10px;border:1px solid #ccc;}td{border:1px solid #ccc;padding:8px;text-align:center;}tr:nth-child(even){background:#f9f9f9;}.pass{color:green;font-weight:bold;}.fail{color:red;font-weight:bold;}</style></head><body>")
            .append("<h2>Test Report - Question 14: ArraySum</h2>")
            .append("<table><tr><th>Given</th><th>When</th><th>Actual Result</th><th>Expect Result</th><th>Pass/Fail</th></tr>");
        for (String[] r : results) {
            String css = r[4].equals("Pass") ? "pass" : "fail";
            html.append("<tr><td>").append(r[0]).append("</td><td>").append(r[1]).append("</td><td>").append(r[2]).append("</td><td>").append(r[3]).append("</td><td class='").append(css).append("'>").append(r[4]).append("</td></tr>");
        }
        html.append("</table></body></html>");
        try (FileWriter fw = new FileWriter("test-report-q14.html")) { fw.write(html.toString()); }
        System.out.println(">>> Report saved: test-report-q14.html");
    }
}