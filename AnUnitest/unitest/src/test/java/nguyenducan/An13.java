package nguyenducan;

import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class An13 {

    static List<String[]> results = new ArrayList<>();

    static void check(String given, String when, String actual, String expected) {
        String pass = actual.equals(expected) ? "Pass" : "Fail";
        results.add(new String[]{given, when, actual, expected, pass});
    }

    @Test
    void testTinhThu() {
        Advance7 x = new Advance7();
        int result = x.tinhThu(6, 4, 2020);
        check("ngay=6, thang=4, nam=2020", "tinhThu(6,4,2020)", String.valueOf(result), "2");
        assertEquals(2, result);
    }

    @Test
    void testInvalidDay() {
        Advance7 x = new Advance7();
        int result = x.tinhThu(35, 6, 2019);
        check("ngay=35, thang=6, nam=2019", "tinhThu(35,6,2019)", String.valueOf(result), "0");
        assertEquals(0, result);
    }

    @Test
    void testInvalidMonth() {
        Advance7 x = new Advance7();
        int result = x.tinhThu(19, 35, 2020);
        check("ngay=19, thang=35, nam=2020", "tinhThu(19,35,2020)", String.valueOf(result), "0");
        assertEquals(0, result);
    }

    @Test
    void testNegativeDay() {
        Advance7 x = new Advance7();
        int result = x.tinhThu(-19, 35, 2020);
        check("ngay=-19, thang=35, nam=2020", "tinhThu(-19,35,2020)", String.valueOf(result), "0");
        assertEquals(0, result);
    }

    @Test
    void testNegativeMonth() {
        Advance7 x = new Advance7();
        int result = x.tinhThu(19, -9, 2020);
        check("ngay=19, thang=-9, nam=2020", "tinhThu(19,-9,2020)", String.valueOf(result), "0");
        assertEquals(0, result);
    }

    @Test
    void testNegativeYear() {
        Advance7 x = new Advance7();
        int result = x.tinhThu(19, 9, -2020);
        check("ngay=19, thang=9, nam=-2020", "tinhThu(19,9,-2020)", String.valueOf(result), "0");
        assertEquals(0, result);
    }

    @AfterAll
    static void exportReport() throws IOException {
        StringBuilder html = new StringBuilder();
        html.append("<!DOCTYPE html><html><head><meta charset='UTF-8'><style>body{font-family:Arial;padding:30px;}h2{color:#333;}table{border-collapse:collapse;width:100%;}th{background:#4CAF50;color:white;padding:10px;border:1px solid #ccc;}td{border:1px solid #ccc;padding:8px;text-align:center;}tr:nth-child(even){background:#f9f9f9;}.pass{color:green;font-weight:bold;}.fail{color:red;font-weight:bold;}</style></head><body>")
            .append("<h2>Test Report - Question 13: Advance7 (tinhThu)</h2>")
            .append("<table><tr><th>Given</th><th>When</th><th>Actual Result</th><th>Expect Result</th><th>Pass/Fail</th></tr>");
        for (String[] r : results) {
            String css = r[4].equals("Pass") ? "pass" : "fail";
            html.append("<tr><td>").append(r[0]).append("</td><td>").append(r[1]).append("</td><td>").append(r[2]).append("</td><td>").append(r[3]).append("</td><td class='").append(css).append("'>").append(r[4]).append("</td></tr>");
        }
        html.append("</table></body></html>");
        try (FileWriter fw = new FileWriter("test-report-q13.html")) { fw.write(html.toString()); }
        System.out.println(">>> Report saved: test-report-q13.html");
    }
}