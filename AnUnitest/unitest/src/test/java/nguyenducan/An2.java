package nguyenducan;

import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.assertEquals;

import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class An2 {

    static List<String[]> results = new ArrayList<>();

    void check(String given, String when, int actual, int expected) {
        String pass = actual == expected ? "Pass" : "Fail";
        results.add(new String[]{given, when, String.valueOf(actual), String.valueOf(expected), pass});
    }

    @Test
    void testFirstNumberIsMax() {
        MaxNumber1 finder = new MaxNumber1();
        finder.setNumber1(10); finder.setNumber2(5); finder.setNumber3(3);
        int result = finder.max3();
        check("number1=10, number2=5, number3=3", "max3()", result, 10);
        assertEquals(10, result);
    }

    @Test
    void testSecondNumberIsMax() {
        MaxNumber1 finder = new MaxNumber1();
        finder.setNumber1(5); finder.setNumber2(10); finder.setNumber3(3);
        int result = finder.max3();
        check("number1=5, number2=10, number3=3", "max3()", result, 10);
        assertEquals(10, result);
    }

    @Test
    void testThirdNumberIsMax() {
        MaxNumber1 finder = new MaxNumber1();
        finder.setNumber1(5); finder.setNumber2(3); finder.setNumber3(10);
        int result = finder.max3();
        check("number1=5, number2=3, number3=10", "max3()", result, 10);
        assertEquals(10, result);
    }

    @Test
    void testAllEqual() {
        MaxNumber1 finder = new MaxNumber1();
        finder.setNumber1(7); finder.setNumber2(7); finder.setNumber3(7);
        int result = finder.max3();
        check("number1=7, number2=7, number3=7", "max3()", result, 7);
        assertEquals(7, result);
    }

    @Test
    void testFirstSecondEqual() {
        MaxNumber1 finder = new MaxNumber1();
        finder.setNumber1(10); finder.setNumber2(10); finder.setNumber3(5);
        int result = finder.max3();
        check("number1=10, number2=10, number3=5", "max3()", result, 10);
        assertEquals(10, result);
    }

    @Test
    void testSecondThirdEqual() {
        MaxNumber1 finder = new MaxNumber1();
        finder.setNumber1(5); finder.setNumber2(10); finder.setNumber3(10);
        int result = finder.max3();
        check("number1=5, number2=10, number3=10", "max3()", result, 10);
        assertEquals(10, result);
    }

    @Test
    void testFirstThirdEqual() {
        MaxNumber1 finder = new MaxNumber1();
        finder.setNumber1(10); finder.setNumber2(5); finder.setNumber3(10);
        int result = finder.max3();
        check("number1=10, number2=5, number3=10", "max3()", result, 10);
        assertEquals(10, result);
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
            .append("<h2>Test Report - Question 2: MaxNumber1</h2>")
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

        try (FileWriter fw = new FileWriter("test-report-q2.html")) {
            fw.write(html.toString());
        }
        System.out.println(">>> Report saved: test-report-q2.html");
    }
}