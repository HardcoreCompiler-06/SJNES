package nguyenducan;

import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class An5 {

    static List<String[]> results = new ArrayList<>();

    static void check(String given, String when, boolean actual, boolean expected) {
        String pass = actual == expected ? "Pass" : "Fail";
        results.add(new String[]{given, when, String.valueOf(actual), String.valueOf(expected), pass});
    }

    @Test
    void testNumber1Greater() {
        Sort2.number1 = 10;
        Sort2.number2 = 5;
        Sort2.sortDesc();
        boolean result = Sort2.number1 == 10 && Sort2.number2 == 5;
        check("number1=10, number2=5", "sortDesc()", result, true);
        assertTrue(result);
    }

    @Test
    void testNumber1Less() {
        Sort2.number1 = 3;
        Sort2.number2 = 8;
        Sort2.sortDesc();
        boolean result = Sort2.number1 == 8 && Sort2.number2 == 3;
        check("number1=3, number2=8", "sortDesc()", result, true);
        assertTrue(result);
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
            .append("<h2>Test Report - Question 5: Sort2</h2>")
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

        try (FileWriter fw = new FileWriter("test-report-q5.html")) {
            fw.write(html.toString());
        }
        System.out.println(">>> Report saved: test-report-q5.html");
    }
}