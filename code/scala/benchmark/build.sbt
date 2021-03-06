import Dependencies._

lazy val root = (project in file(".")).
  settings(
    inThisBuild(List(
      organization := "com.example",
      scalaVersion := "2.12.3",
      version      := "0.1.0-SNAPSHOT"
    )),
    name := "Hello",
    libraryDependencies += scalaTest % Test,
    fork in run := true,
    javaOptions in run += "-XX:-UseGCOverheadLimit",
    javaOptions in run += "-Xmx4g"
  )
