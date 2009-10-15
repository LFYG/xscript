<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:math="http://exslt.org/math"
    extension-element-prefixes="math"
>

<xsl:variable name="collect-time" select="/page/response-time/@collect-time"/>
<xsl:template match="/page/response-time">
    <html>
        <head>
	    <title>Xscript5 response time statistics</title>
        </head>
        <body>
	    <h2>Xscript5 response time statistics</h2>

	    <div style="margin-left:5px">Collection time: <xsl:value-of select="$collect-time"/> sec<br/><br/></div>
	    
	    <table border="1" cellpadding="5px">
	        <tr align="center">
		    <td>Status</td>
		    <td>Auth status</td>
		    <td>Count</td>
		    <td>Avg time</td>
		    <td>Min time</td>
		    <td>Max time</td>
<!--		    <td>Total response</td>  -->
		    <td>RPS</td>
		</tr>
		<xsl:if test="$collect-time > 0">
		    <xsl:apply-templates select="status"/>
		</xsl:if>
	    </table>
	    <br/>
	    <div style="margin-left:5px">* Time data in microseconds</div>
	</body>
    </html>
</xsl:template>

<xsl:template match="status">
    <tr colspan="8"><td></td></tr>
    <tr align="center" style="font-weight:normal;background-color:#dddddd">
        <td><xsl:value-of select="@code"/></td>
	<td>all</td>
        <td><xsl:value-of select="sum(child::point/@count)"/></td>
	<xsl:variable name="sum" select="sum(child::point/@count)"/>
	<td>
	    <xsl:choose>
	        <xsl:when test="$sum = 0">
	            <xsl:value-of select="0"/>
	        </xsl:when>
		<xsl:otherwise>
		    <xsl:value-of select="floor(sum(child::point/@total) div $sum)"/>
		</xsl:otherwise>
	    </xsl:choose>
        </td>
        <td><xsl:value-of select="math:min(child::point/@min)"/></td>
        <td><xsl:value-of select="math:max(child::point/@max)"/></td>	
<!--        <td><xsl:value-of select="sum(child::point/@total)"/></td>  -->
        <td><xsl:value-of select="format-number(sum(child::point/@count) div $collect-time, '#.###')"/></td>
    </tr>
 
    <xsl:apply-templates select="point"/>

</xsl:template>

<xsl:template match="point">
    <tr align="center">
        <td><xsl:value-of select="../@code"/></td>
	<td><xsl:value-of select="@auth-type"/></td>
	<td><xsl:value-of select="@count"/></td>        
	<td><xsl:value-of select="@avg"/></td>
        <td><xsl:value-of select="@min"/></td>
        <td><xsl:value-of select="@max"/></td>
<!--        <td><xsl:value-of select="@total"/></td>   -->
	<td><xsl:value-of select="format-number(@count div $collect-time, '#.###')"/></td>
    </tr>
</xsl:template>

</xsl:stylesheet>