package nguyenducan;

import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.assertEquals;

import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class An3 {

    static List<String[]> results = new ArrayList<>();

    void check(String given, String when, int actual, int expected) {
        String pass = actual == expected ? "Pass" : "Fail";
        results.add(new String[]{given, when, String.valueOf(actual), String.valueOf(expected), pass});
    }

    @Test
    void testFirstNumberIsGreater() {
        MaxNumber2 finder = new MaxNumber2(10, 5);
        int result = finder.max2();
        check("number1=10, number2=5", "max2()", result, 10);
        assertEquals(10, result);
    }

    @Test
    void testSecondNumberIsGreater() {
        MaxNumber2 finder = new MaxNumber2(5, 10);
        int result = finder.max2();
        check("number1=5, number2=10", "max2()", result, 10);
        assertEquals(10, result);
    }

    @Test
    void testEqual() {
        MaxNumber2 finder = new MaxNumber2(7, 7);
        int result = finder.max2();
        check("number1=7, number2=7", "max2()", result, 7);
        assertEquals(7, result);
    }

    @AfterAll
    static void exportReport() throws IOException {
        StringBuilder html = new StringBuilder();
        html.append("<!DOCTYPE html><html><head><meta charset='UTF-8'>")
            .append("<style>")
            .append("body{font-family:Arial;padding:30px;}")
            .append("h2{color:#333;}")
            .append("table{border-collapse:collapse;width:100%;}")
            .append("th{background:#4CAF50;color:white;padding:10px;border:1px solid #ccc;}")
            .append("td{border:1px solid #ccc;padding:8px;text-align:center;}")
            .append("tr:nth-child(even){background:#f9f9f9;}")
            .append(".pass{color:green;font-weight:bold;}")
            .append(".fail{color:red;font-weight:bold;}")
            .append("</style></head><body>")
            .append("<h2>Test Report - Question 3: MaxNumber2</h2>")
            .append("<table>")
            .append("<tr><th>Given</th><th>When</th>")
            .append("<th>Actual Result</th><th>Expect Result</th><th>Pass/Fail</th></tr>");

        for (String[] r : results) {
            String css = r[4].equals("Pass") ? "pass" : "fail";
            html.append("<tr>")
                .append("<td>").append(r[0]).append("</td>")
                .append("<td>").append(r[1]).append("</td>")
                .append("<td>").append(r[2]).append("</td>")
                .append("<td>").append(r[3]).append("</td>")
                .append("<td class='").append(css).append("'>").append(r[4]).append("</td>")
                .append("</tr>");
        }

        html.append("</table></body></html>");

        try (FileWriter fw = new FileWriter("test-report-q3.html")) {
            fw.write(html.toString());
        }
        System.out.println(">>> Report saved: test-report-q3.html");
    }
}