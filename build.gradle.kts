import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
	id("net.fabricmc.fabric-loom-remap")
	`maven-publish`
	id("org.jetbrains.kotlin.jvm") version "2.3.21"
}

version = providers.gradleProperty("mod_version").get()
group = providers.gradleProperty("maven_group").get()

loom {
	accessWidenerPath = file("src/main/resources/nightjava.accesswidener")
}

repositories {
}

dependencies {
	minecraft("com.mojang:minecraft:${providers.gradleProperty("minecraft_version").get()}")
	mappings("net.fabricmc:yarn:${providers.gradleProperty("yarn_mappings").get()}:v2")
	modImplementation("net.fabricmc:fabric-loader:${providers.gradleProperty("loader_version").get()}")
	modImplementation("net.fabricmc.fabric-api:fabric-api:${providers.gradleProperty("fabric_api_version").get()}")
	modImplementation("net.fabricmc:fabric-language-kotlin:${providers.gradleProperty("fabric_kotlin_version").get()}")
}

val cmakeDir = file("native")
val nativeBuildDir = file("native/build")

tasks.register<Exec>("cmakeConfigure") {
	workingDir = cmakeDir
	val isWindows = System.getProperty("os.name").lowercase().contains("win")
	val cmakeGenerator = if (isWindows) "-G" to "MinGW Makefiles" else null
	
	commandLine = listOfNotNull(
		"cmake", "-B", nativeBuildDir.absolutePath
	) + if (cmakeGenerator != null) listOf(cmakeGenerator.first, cmakeGenerator.second, "-DCMAKE_CXX_COMPILER=g++") else emptyList()
	
	inputs.dir(file("$cmakeDir/src"))
	inputs.dir(file("$cmakeDir/include"))
	inputs.file(file("$cmakeDir/CMakeLists.txt"))
	outputs.dir(nativeBuildDir)
}

tasks.register<Exec>("cmakeBuild") {
	dependsOn("cmakeConfigure")
	workingDir = cmakeDir
	commandLine = listOf("cmake", "--build", nativeBuildDir.absolutePath)
	
	outputs.dir(file("$nativeBuildDir"))
}

tasks.processResources {
	dependsOn("cmakeBuild")
	val version = version
	inputs.property("version", version)

	filesMatching("fabric.mod.json") {
		expand("version" to version)
	}

	from(nativeBuildDir) {
		include("*.dll")
		into("natives/windows/x64")
	}
}

tasks.withType<JavaCompile>().configureEach {
	options.release = 21
}

kotlin {
	compilerOptions {
		jvmTarget = JvmTarget.JVM_21
	}
}

java {
	withSourcesJar()
	sourceCompatibility = JavaVersion.VERSION_21
	targetCompatibility = JavaVersion.VERSION_21
}

tasks.jar {
	val projectName = project.name
	inputs.property("projectName", projectName)

	from("LICENSE") {
		rename { "${it}_$projectName" }
	}
}

publishing {
	publications {
		register<MavenPublication>("mavenJava") {
			from(components["java"])
		}
	}

	repositories {
	}
}
